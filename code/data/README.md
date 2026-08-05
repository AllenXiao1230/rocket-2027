# Data

| Directory | Responsibility | Git policy |
| --- | --- | --- |
| `samples/` | 經刻意保留、依 sensor/link 分類的 diagnostic recordings | 可提交，但須先審查 |
| `legacy_runs/` | 從舊 source project 分離出的歷史 evidence | 保留原貌；不要當作目前驗證結果 |
| `runs/` | Ground Station 與 diagnostic tools 的 runtime output | `.gitignore` 排除，不直接提交 |

檔名應包含 subsystem 與 capture time，例如 `gps_20260805T143000.csv`。將資料從 `runs/` 移至 `samples/` 前必須確認：

- 沒有 secrets、credentials 或不應分享的個人 GPS 位置；
- 檔案不是截斷、損壞或未完成寫入；
- header/schema 與使用的 protocol/firmware version 可追溯；
- missing/invalid 值仍保持 empty，不可事後轉成假零；
- README 或相鄰 metadata 記錄硬體、日期、測試目的與限制。

任何 sample 都只是對應測試條件下的 evidence，不代表目前 production build 已通過 flight qualification。
