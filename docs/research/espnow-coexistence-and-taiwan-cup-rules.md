# ESP-NOW／SX1280 共存與 2026 台灣盃火箭競賽公開規範查核

查核日：2026-08-25。範圍為機上 ESP32-S3 的 ESP-NOW 與獨立 SX1280 2.4 GHz radio，及 **2026 台灣盃火箭競賽（RTC 2026）已公開的官方資料**。本文不是 NCC 型式認證、飛行許可、射場核准或射程保證。

> **2027 設計邊界（2026-08-25 更新）**：本火箭的目標賽事為 2027 台灣盃。查核時尚未找到 2027 的官方競賽規則、附件或 LRR checklist，因此本文所列 RTC 2026 資料只可作為保守的規劃基線，不能當作 2027 合規聲明。任何 flight configuration 的凍結都必須通過當年度的 Competition Acceptance Gate。

## 結論

**會有干擾風險；不能把 ESP-NOW 與 SX1280 同時運作視為互不影響。** ESP-NOW 是在 Wi-Fi 已啟動後，以 Wi-Fi vendor-specific action frame 傳送的 2.4 GHz 無線鏈路；SX1280 也工作於 2400–2500 MHz。因此若兩者在箭上同時收／發，它們共享 2.4 GHz 這一個 common-band fault domain。ESP-NOW 的「不同 RF IC」不是頻率隔離。

Semtech 的資料顯示 SX1280 對 Wi-Fi 的抗干擾度會隨 LoRa SF、頻寬、Wi-Fi 波形及頻率偏移而改變，並建議避開被佔用的頻段；這是有條件的單一接收機抗擾結果，**不是**對同一箭體近距離雙發射、雙向封包遺失率或 ESP-NOW 接收端的保證。[Semtech FAQ：SX1280 Wi-Fi immunity](https://www.semtech.com/design-support/faq/P60)

建議將本組合定義為「920–925 MHz Main + 2.4 GHz Backup，而 ESP-NOW 為非飛行期的近距離維護鏈路」。若 ESP-NOW 必須在飛行中啟用，2.4 GHz Backup 在完成下列共存 gate 前只能是測試候選，不可稱為獨立的可靠備援鏈路。

## 1. 官方原始資料可驗證的無線事實

| 可驗證事實 | 對設計的含義 |
| --- | --- |
| ESP-NOW 是 connectionless Wi-Fi；資料封裝為 vendor-specific action frame。傳送前必須先啟動 Wi-Fi。ESP32-S3 的 Wi-Fi 是 2.4 GHz 802.11b/g/n。 | ESP-NOW 不是一個與 Wi-Fi 無關的「低頻內部通訊」；開啟它即啟用 ESP32-S3 的 2.4 GHz Wi-Fi RF。 [Espressif ESP-IDF：ESP32-S3 ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html)，[Espressif ESP32-S3 product page](https://www.espressif.com/en/products/socs/esp32-s3) |
| ESP-NOW peer 的設定頻道必須是本機 Station／SoftAP 所在頻道；頻道不一致是送出失敗的已列舉原因。 | 必須把 ESP-NOW 固定頻道列入整體 frequency plan，不能讓它自動選頻後再假設 SX1280 仍可用。 [ESP-NOW peer/channel API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html) |
| ESP-NOW 的 MAC 層送達成功不保證應用層收達；Espressif 建議在需要時加入 ack、timeout、retry 與 sequence number。 | ESP-NOW 不得作為未量測之飛行 safety／recovery 成功依據；共存試驗應記錄應用層成功率與重傳後延遲，不只看 callback。 [Espressif ESP-NOW reliability note](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html) |
| SX1280 是 2400–2500 MHz transceiver，支援 LoRa、FLRC、(G)FSK；其可用 LoRa 頻寬是 200–1600 kHz。 | ESP-NOW 與 SX1280 明確處於同一 2.4 GHz 頻段；欲選用 SX1280 LoRa 設定時，必須連同中心頻率與頻寬一起鎖定。 [Semtech SX1280 product page](https://www.semtech.com/products/wireless-rf/lora-connect/sx1280) |
| SX1280 對 Wi-Fi 的 immunity 不固定；Semtech 明確說取決於 SF、頻寬、Wi-Fi 類型與頻率 offset，並說一般最佳作法是選擇未被佔用的頻段。 | 不可用「LoRa 很抗 Wi-Fi」作為共存結論；要以選定的 ESP-NOW channel、SX1280 SF/BW、功率和天線實測。 [Semtech FAQ：SX1280 Wi-Fi immunity](https://www.semtech.com/design-support/faq/P60) |

### NCC 的外部法規邊界

NCC 現行《低功率射頻器材技術規範》（LP0002）於 2024-02-06 修正，公開頁面亦提供完整條文與 PDF。既有 2.4 GHz 設計必須依最終調變、6 dB 頻寬、PSD、輸出、天線增益、帶外／混附發射等條件確認適用款項；**「IC 可在 2.4 GHz 工作」不等於整機已合規**。[NCC LP0002（現行條文與 PDF）](https://ncclaw.ncc.gov.tw/FLAW/FLAWDAT0202.aspx?id=FL094278)

本次不把競賽公告視為對 NCC 的豁免，也不由競賽的「通訊系統可自行設計」推導出特定頻率、功率、調變、天線或型式認證已被允許。

## 2. 建議的共存架構與硬性驗證 gate

### 飛行模式

1. **優先方案：ESP-NOW 僅限維修／地面 pre-flight。** 進入發射倒數前關閉 Wi-Fi／ESP-NOW；飛行、下降與回收由 Main 920–925 MHz 和經驗證的 SX1280 任務鏈路負責。這消除 ESP-NOW 在飛行中的在頻發射，但仍要做上電 EMI 與 antenna detuning 測試。
2. **若 ESP-NOW 必須飛行中運作：採軟硬體互鎖，不允許 ESP-NOW TX 與 SX1280 TX 同時發射。** 以一個明確的 radio arbiter／time-slot 表給予 Main telemetry、recovery 及 Backup 的優先權；不可依賴任務排程「通常不會撞到」。
3. 為 ESP-NOW 設定固定、受台灣區域規則約束的 channel，並依現場頻譜量測選擇 SX1280 中心頻率／頻寬。這不是保證兩者不互擾，而是可追溯、可重複的測試前提。ESP-NOW 若同時連到 AP，還必須與 AP 同頻道。 [Espressif ESP-FAQ：ESP-NOW 與 Wi-Fi](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html)
4. 天線、feedline、地平面、電池、開關電源與兩個 radio 都必須使用最終箭體配置測試。沒有 Semtech／Espressif 公開資料可推導出本箭體的最小天線距離、必要濾波值或安全的同時 TX 功率；這三個數值不得臆定。

### 共存 gate（任一失敗即不把 2.4 GHz 當 flight Backup）

| Gate | 要求的實測證據 |
| --- | --- |
| 固定配置 | 記錄 ESP-IDF 版本、ESP-NOW channel/rate/TX power、SX1280 centre frequency/SF/BW/CR/TX power、兩端天線與最終機構照片。 |
| 封包測試矩陣 | 在最終箭體，量測：ESP-NOW TX + SX1280 RX、SX1280 TX + ESP-NOW RX、交錯 TX、兩者 RX，以及 ESP-NOW 關閉基線。每格均以代表性完整 telemetry payload、最大預定 burst 與重傳策略執行。 |
| 指標 | 逐鏈路記錄 application-layer delivery ratio、PER、p95/p99 latency、連續遺失長度、RSSI/SNR（可取得時）與重啟／brownout；不得只以 ESP-NOW MAC callback 或單次通訊成功判定。 |
| 空間／姿態 | 在 launch-site 頻譜量測後，做最壞相對方位、旋轉／極化、預定距離與其他合法 2.4 GHz 發射源存在時的試驗。 |
| 法規／競賽 | 在鎖定 waveform 後取得 NCC 適用性與參賽年度的書面 RF 規定；發射許可審查前提交主辦方要求的最終版本。 |

## 3. 2026 台灣盃：官方公開規範的可驗證內容

### 已公開且可引用

| 主題 | 2026 官方資料可驗證內容 |
| --- | --- |
| 主辦與組別 | 國家太空中心與屏東縣政府共同主辦；組別為學生 1K、學生 3K、社會 3K，使用公版推進系統，目標高度分別為 1 km／3 km／3 km。 [RTC 官方活動簡章](https://www.rockettaiwancup.tw/) |
| 航電、通訊、回收 | 2026 官方政府新聞明載：各隊自行設計箭體、鼻錐、尾翼、**航電、酬載及回收系統**，並接受飛行安全、系統工程及發射作業的完整驗證；通訊是整合工程的一部分。 [行政院南部聯合服務中心：RTC 2026 閉幕](https://eysc.ey.gov.tw/Page/DAB11F5DBDC11168/a262377d-02ce-4617-b29f-90463500c638) |
| 發射／安全 | 同一官方新聞證實：21 組決賽隊伍均須通過發射許可審查，20 組成功飛行；公開競賽頁也列出發射備便報告（LRR）審查與射場安全集訓／初級火箭發射執照測驗。 [行政院官方新聞](https://eysc.ey.gov.tw/Page/DAB11F5DBDC11168/a262377d-02ce-4617-b29f-90463500c638)，[RTC 官方時程](https://www.rockettaiwancup.tw/) |
| 評分／安全重點 | 2026 官方政府新聞表示，除高度與任務完成度，評審重視系統整合、安全設計、風險管理與團隊合作。這支持把 RF 共存與 recovery 失效模式列入設計／LRR 證據。 [行政院官方新聞](https://eysc.ey.gov.tw/Page/DAB11F5DBDC11168/a262377d-02ce-4617-b29f-90463500c638) |

### 未能取得的 RF 專項規範：不可假設

截至查核日，已檢索 RTC 官方網站的活動簡章、報名／課程／公告頁，以及 TASA、行政院等 2026 官方公告；**未找到可公開下載的 2026 完整競賽規則書、附件八「火箭發射許可審查項目」或含 RF 專章的 LRR checklist。** 官方首頁只顯示「附件八」須有至少 16 項核可才有材料補助，並列出 LRR 審查期程，沒有公開附件內容或 RF 參數表。[RTC 官方活動簡章與時程](https://www.rockettaiwancup.tw/)

這是「公開來源未能取得」的結論，**不是**宣稱主辦方不存在內部／參賽隊取得的文件。該站也表示公開資訊可能延遲或異動，並要求以主辦方正式公告為準。[RTC 官方條款與條件](https://www.rockettaiwancup.tw/blank-2)

因而下列項目一律為 **未知，不可由公開 2026 資料推定為允許**：

- ESP-NOW、SX1280 LoRa、2.4 GHz 或任一頻點是否准用；頻率協調、排程、頻寬、調變、空中時間、TX 功率、EIRP、天線型式／數量的上限。
- 遙測、即時影像、飛行控制／指令、地面站、備援 radio 是否可同時使用，以及是否需先行登錄、頻率核配或現場關閉。
- 失聯／recovery 的最低功能、雙重回收、定位 beacon、pyro／deployment 的獨立性與測試／文件門檻。
- 可攜帶器材、資料紀錄、EMI、電池與其他 avionics 的限制，及發射許可 16 項中哪些是硬性項目。

最新可公開查到的完整規則 PDF 是 **2025** 版本；它不是 2026 規則，本文件不以其內容替 2026 作任何 RF 或安全判定。[TASA：2025 競賽規則說明 v4（僅作歷史來源辨識）](https://www.tasa.org.tw/files/proxy/%E9%99%84%E4%BB%B6%E4%B8%89_2025_%E5%8F%B0%E7%81%A3%E7%9B%83%E7%81%AB%E7%AE%AD%E7%AB%B6%E8%B3%BD_%E7%AB%B6%E8%B3%BD%E8%A6%8F%E5%89%87%E8%AA%AA%E6%98%8E_v4.pdf_56490787-f9a4-4207-9f06-6c6f5a0e35f7.pdf)

## 4. 應向主辦方正式索取的資料

向 RTC 官網列示的聯絡信箱 `rockettaiwancup2026@gmail.com` 提出書面索取，目的寫明為「航電／通訊設計與 LRR 合規核對」，並請主辦方回覆或提供以下**適用年份與版本號**的正式文件：

1. 完整競賽簡章、所有附件，尤其附件八發射許可審查／LRR checklist。
2. RF／telemetry／影像／command 的允許頻段、功率／EIRP、頻寬／調變、airtime、radio 與天線數量、頻率協調及地面站規則。
3. ESP-NOW／Wi-Fi、2.4 GHz LoRa 與雙 radio 是否允許在箭上；是否允許同時發射及何種階段必須關閉。
4. recovery、定位、失聯、電源隔離、飛行前測試、資料記錄及安全中止的最低要求與驗證格式。
5. 無線器材的 NCC 型式認證／核可責任歸屬，及是否需在 LRR 前繳交型號、設定、功率、頻點與測試證據。

在收到正式版本前，設計文件應寫成「**2.4 GHz 選擇與 ESP-NOW flight use 均待主辦方與 NCC 適用性確認**」，而非稱為已符合 2026 台灣盃規則。

## 來源與證據界線

外部來源僅採官方競賽／政府／主管機關，或 IC 原廠 Espressif、Semtech。元件規格與 Semtech Wi-Fi immunity 資料可用於制定測試，但不能替代最終箭體、地面站、射場與全飛行剖面的實測，也不能替代主辦方的書面發射許可。
