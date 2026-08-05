import threading
import time
import unittest

from rocket_ground_station.transport.serial_worker import (
    SerialEventKind,
    SerialWorker,
    SerialWorkerConfig,
)


class FakeSerial:
    def __init__(self, chunks=()):
        self._chunks = list(chunks)
        self.writes = []
        self.is_open = True
        self.closed = False
        self.dtr = True
        self.rts = True

    @property
    def in_waiting(self):
        return len(self._chunks[0]) if self._chunks else 0

    def read(self, size):
        chunk = self._chunks.pop(0)
        self.assert_read_size = size
        return chunk

    def write(self, data):
        self.writes.append(bytes(data))
        return len(data)

    def close(self):
        self.closed = True
        self.is_open = False


class FailingWriteSerial(FakeSerial):
    def write(self, data):
        self.writes.append(bytes(data))
        raise OSError("USB serial disconnected during write")


class SerialWorkerTests(unittest.TestCase):
    def test_worker_emits_raw_bytes_and_transport_events(self):
        raw = b"\xA5\x5A\x02\x40not-parsed"
        fake = FakeSerial([raw])
        calls = []
        received = threading.Event()
        factory_kwargs = {}

        def factory(**kwargs):
            factory_kwargs.update(kwargs)
            return fake

        def on_event(event):
            calls.append(event)
            if event.kind == SerialEventKind.BYTES_RECEIVED:
                received.set()

        worker = SerialWorker(
            SerialWorkerConfig("/dev/fake", poll_interval_s=0.001),
            on_event,
            serial_factory=factory,
        )
        worker.start()
        self.assertTrue(received.wait(1.0))
        self.assertTrue(worker.stop(1.0))

        self.assertEqual(factory_kwargs["port"], "/dev/fake")
        self.assertEqual(factory_kwargs["baudrate"], 115200)
        self.assertFalse(fake.dtr)
        self.assertFalse(fake.rts)
        self.assertTrue(fake.closed)
        self.assertEqual(
            [event.kind for event in calls],
            [
                SerialEventKind.CONNECTED,
                SerialEventKind.BYTES_RECEIVED,
                SerialEventKind.DISCONNECTED,
            ],
        )
        self.assertEqual(calls[1].data, raw)

    def test_worker_writes_only_bytes_while_connected(self):
        fake = FakeSerial()
        connected = threading.Event()

        def on_event(event):
            if event.kind == SerialEventKind.CONNECTED:
                connected.set()

        worker = SerialWorker(
            SerialWorkerConfig("/dev/fake", poll_interval_s=0.001),
            on_event,
            serial_factory=lambda **_: fake,
        )
        self.assertFalse(worker.write(b"before-connect"))
        worker.start()
        self.assertTrue(connected.wait(1.0))

        self.assertTrue(worker.write(memoryview(b"RTC_SYNC:1\n")))
        with self.assertRaises(TypeError):
            worker.write("text")
        with self.assertRaises(ValueError):
            worker.write(b"")
        self.assertTrue(worker.stop(1.0))
        self.assertEqual(fake.writes, [b"RTC_SYNC:1\n"])

    def test_write_failure_stops_closes_and_emits_disconnect(self):
        fake = FailingWriteSerial()
        calls = []
        connected = threading.Event()
        disconnected = threading.Event()

        def on_event(event):
            calls.append(event)
            if event.kind == SerialEventKind.CONNECTED:
                connected.set()
            elif event.kind == SerialEventKind.DISCONNECTED:
                disconnected.set()

        worker = SerialWorker(
            SerialWorkerConfig("/dev/fake", poll_interval_s=0.001),
            on_event,
            serial_factory=lambda **_: fake,
        )
        self.addCleanup(worker.stop, 1.0)
        worker.start()
        self.assertTrue(connected.wait(1.0))

        self.assertFalse(worker.write(b"RTC_SYNC:1\n"))
        self.assertTrue(disconnected.wait(1.0))

        deadline = time.monotonic() + 1.0
        while worker.is_running and time.monotonic() < deadline:
            time.sleep(0.001)

        self.assertFalse(worker.is_running)
        self.assertFalse(worker.is_connected)
        self.assertTrue(fake.closed)
        self.assertEqual(fake.writes, [b"RTC_SYNC:1\n"])
        self.assertEqual(
            [event.kind for event in calls],
            [
                SerialEventKind.CONNECTED,
                SerialEventKind.ERROR,
                SerialEventKind.DISCONNECTED,
            ],
        )
        self.assertIn("Serial write failed", calls[1].message)

    def test_open_failure_is_reported_without_fake_disconnect(self):
        calls = []

        def fail_factory(**_):
            raise OSError("missing port")

        worker = SerialWorker(
            SerialWorkerConfig("/dev/missing"),
            calls.append,
            serial_factory=fail_factory,
        )
        worker.start()

        deadline = time.monotonic() + 1.0
        while worker.is_running and time.monotonic() < deadline:
            time.sleep(0.001)
        worker.stop(1.0)

        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0].kind, SerialEventKind.ERROR)
        self.assertIn("Serial open failed", calls[0].message)
        self.assertFalse(worker.is_connected)

    def test_event_handler_failure_does_not_kill_transport(self):
        fake = FakeSerial([b"abc"])
        connected = threading.Event()

        def bad_handler(event):
            if event.kind == SerialEventKind.CONNECTED:
                connected.set()
            raise RuntimeError("consumer failed")

        worker = SerialWorker(
            SerialWorkerConfig("/dev/fake", poll_interval_s=0.001),
            bad_handler,
            serial_factory=lambda **_: fake,
        )
        worker.start()
        self.assertTrue(connected.wait(1.0))
        self.assertTrue(worker.stop(1.0))
        self.assertTrue(fake.closed)


if __name__ == "__main__":
    unittest.main()
