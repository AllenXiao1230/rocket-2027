# Rocket Avionics

Rocket 2027 的雙飛控航電語彙。此文件只定義系統概念；硬體選型、門檻值與實作細節以 Architecture Baseline 與後續 ADR 為準。

## Flight computers

**Main Flight Computer**:
正常任務中持有 Recovery Authority 的獨立飛控 fault domain。它擁有自己的感測、記錄、RF、電源與 recovery path。
_Avoid_: 主板、primary MCU

**Backup Flight Computer**:
可獨立估測狀態、記錄、通訊與執行 recovery 的第二個飛控 fault domain；僅在合法 authority transfer 後能執行 recovery。
_Avoid_: 簡化備援板、secondary sensor node

**Local Recovery Decision**:
單一 Flight Computer 由自己的感測與狀態推導的 recovery 判定。Peer 資料只可作 cross-check，不能覆蓋它。
_Avoid_: 共識判定、peer override

**Flight Core**:
不依賴 Main 或 Backup 板件的 flight、recovery 與 protocol 純邏輯。它由兩個 deployable flight targets 共用，但不擁有硬體、校正或任務執行期狀態。
_Avoid_: shared flight computer、common runtime

**Deployable Flight Target**:
Main 或 Backup 的獨立可建置、可燒錄 firmware 與其 hardware adapter。它是獨立 fault domain 的軟體邊界，不是同一 binary 的模式旗標。
_Avoid_: build profile、backup mode

**Shared-Enclosure Dual-FC Assembly**:
同一密封航電艙內、由隔板分區的兩塊獨立 Flight Computer PCB 與各自獨立 Power Board 所構成的組件。它維持電氣與 recovery path 的分離，但 enclosure、機械、熱、進水與環境載荷仍是共同風險。
_Avoid_: single flight PCB、fully independent enclosure

**Flight-Intent Hardware Revision**:
從第一版 PCB 起即以最終飛行所需的功能、隔離與可驗證性為目標的硬體 revision。它仍可依測試結果修訂，但沒有與 flight configuration 無關的 bring-up-only 架構。
_Avoid_: disposable bring-up board、late flight integration

## Recovery

**Recovery Authority**:
在某次 mission 中被允許使 recovery command 穿過硬體 interlock 的唯一所有權。它由完整 authority state machine 表達，並可在 Backup takeover 後鎖定至 mission 結束。
_Avoid_: `backup_has_authority` boolean、active controller

**Arming Interlock**:
進入 `ARMED` 前同時要求實體 Safe/Arm 條件、已驗證 Ground Station ARM 請求與 Pre-Arm Inhibit Consistency 的授權邊界。它不是 recovery driver 的直接控制訊號，任一條件不成立即保持 inhibit。
_Avoid_: arm GPIO、software-only arm

**Pre-Arm Inhibit Consistency**:
Main 與 Backup 的 Local Inhibit Readback 都存在、互不矛盾且與實體 Safe/Arm 接點一致的 pre-arm 條件。任何缺失、矛盾或不符都會拒絕 ARM 並保留 `READY`。
_Avoid_: Main-only inhibit approval、Ground Station override、warning-only mismatch

**Fresh Authorization After Inhibit Fault**:
發生 Pre-Arm Inhibit Consistency 不一致後，即使物理原因已排除，下一次 ARM 仍必須建立新的 Authorized Ground Command Session，並重新完成全部雙飛控 pre-arm 檢查的規則。
_Avoid_: retained ARM grant、automatic re-arm、inspection-only clearance

**Authority Latch**:
在合法 Backup takeover 後保持至 mission end 的獨立硬體 arbitration state。它只能由 Main-failure evidence、Backup recovery capability 與有效 ARM/interlock 條件共同促成，不能由 MCU reboot、單一 firmware 或 Ground Station 直接重置。
_Avoid_: GPIO enable、resettable software owner

**Independent Recovery Power Branch**:
由所屬 Main 或 Backup 2S pack 經自身保護與 driver 供應的 recovery 能源路徑。它不與另一飛控共用 battery 或 recovery branch。
_Avoid_: common recovery battery、flight-core rail

**Keyed Safe/Arm Interface**:
具兩條獨立接點的實體 Safe/Arm 介面：一條維持 recovery energy 的硬體 inhibit，另一條讓兩台 Flight Computer 讀取實體狀態。兩條接點可被交叉檢查。
_Avoid_: MCU-only arm、single logic switch

**Independent 2S Power Domain**:
Main 或 Backup 自己的 2S 18650 pack、charger with PowerPath、BMS、NTC、XT30 與下游 rails 所構成的能源 fault domain。
_Avoid_: shared battery pack、shared power rail

**Main Failure Model**:
以 state 與持續時間處理的多訊號評估，區分硬故障、強證據與輔助證據；只有已定義的組合才能啟動 authority transfer。單次 reset 或單一 RF/SD 失效不構成 Main failure。
_Avoid_: equal N-of-M vote、heartbeat-only failure

**Independent Recovery-Path Failure**:
指向 Main recovery path 實際失效的獨立證據，而非單純未觀察到 deployment effectiveness。它與已確認 Main failure、Backup recovery capability 及 Backup Recovery Authority 共同構成 Backup 使用對應 channel 的必要條件。
_Avoid_: effect-not-confirmed retry、automatic duplicate command

**Ground Test Session**:
由獨立實體 Test key/jumper 與 Ground Station 明確授權共同建立的非飛行測試狀態。它不等同於 `ARMED`，且未建立時所有 test request 必須被拒絕。
_Avoid_: UI test toggle、armed test

**Authorized Ground Command Session**:
以 challenge-response、一次性 nonce、source identity、counter、版本與完整性保護建立的短暫 Ground Station command session，可經有線 service port 或 RF command path 送達。它可授權 ARM 或 Test 類狀態轉換，並會逾時及留下明確拒絕紀錄。
_Avoid_: fixed PIN、connected serial port、UI confirmation、telemetry bytes

**Dual Acceptance**:
Main 與 Backup 在限定時間內分別成功驗證同一 Ground command session、nonce 與命令，且各自 preflight/interlock 皆通過的共同接受規則。任何拒絕或逾時都保持 inhibit。
_Avoid_: Main-only acceptance、either-controller acceptance

**Ground Command Window**:
Ground command 只可在 `SAFE`、`READY` 或已建立的 Ground Test Session 接受的任務階段。`LAUNCH_CONFIRMED` 後所有 ARM、Test 與 disarm command 都必須拒絕。
_Avoid_: in-flight RF override、post-launch disarm

**Recovery-Capable**:
Flight Computer 具備足以安全處理 recovery 的本地感測、健康、channel 與 interlock 條件。它是 Backup 取得 authority 的必要條件，不等同於已持有 authority。
_Avoid_: alive、online

**GNSS-Optional Recovery Capability**:
Backup 在共享 GNSS 或其 power MUX 不可用時，仍可依自身 IMU、barometer 與 flight state 保持 Recovery-Capable 的性質。GNSS 失效須被記錄與下傳，但不能成為阻止合法 recovery 的條件。
_Avoid_: GNSS-gated recovery、position-required authority

**Armed GNSS Configuration Lock**:
共享 GNSS 的 power 與 MUX configuration 僅可在 `SAFE` 建立，進入 `ARMED` 後保持鎖定至 mission end 的規則。它將 GNSS 視為非 recovery prerequisite，避免 in-flight switching 成為共同失效源。
_Avoid_: in-flight GNSS reset、automatic MUX cycling

**Safe-Only GNSS Service**:
只在 `SAFE` 且經明確 Wired-Only Safe GNSS Service 後，才可調整共享 GNSS 的 power 或 MUX configuration 的規則。`READY` 僅可重新讀取與診斷，不能改變 configuration。
_Avoid_: READY auto-cycle、Backup MUX control、in-flight GNSS service

**Wired-Only Safe GNSS Service**:
以實體有線 service port、`SAFE` 狀態與 authenticated service action 三者共同授權的共享 GNSS 維修邊界。RF 與 Ground-Only Maintenance Link 都不得用於改變 GNSS power 或 MUX configuration。
_Avoid_: RF GNSS service、wire-or-RF maintenance、unauthenticated jumper-only service

**Recovery Channel**:
一個可一次性執行的 Drogue 或 Main recovery command path，其 lifecycle 為 `INHIBITED → AVAILABLE → REQUESTED → COMMAND_EXECUTED → LOCKED_OUT`。
_Avoid_: output pin、deployment flag

**Recovery Manager**:
驗證 deployment request、authority、flight phase、channel、interlock 與 request age 的高優先級邊界；只有它可以向 recovery hardware driver 提出執行命令。
_Avoid_: general flight task、GPIO helper

**Physical Deployment Confirmation**:
由電氣回應與飛行動態推得的降落傘實體展開證據。它不同於 `COMMAND_EXECUTED`，後者只表示電氣命令已送出。
_Avoid_: deploy GPIO high、command success

**Actuator-Neutral Recovery Interface**:
在未選定實體 release device 前，Recovery Channel 與未來 actuator 之間預留的受 inhibit 保護介面。它可包含 Provisionally Inhibited Recovery Driver，但不代表已有 flight-ready actuator 或獲准的 deployment configuration。
_Avoid_: dummy deployment output、selected actuator、live test connector

**Provisionally Inhibited Recovery Driver**:
已安裝於 Actuator-Neutral Recovery Interface、但由獨立硬體 inhibit 保持不可輸出且未接實體 actuator 的暫定 driver。它用於整合與可測性預留，不是 flight-ready deployment path。
_Avoid_: armed generic output、connected TBD actuator、flight-ready driver

**Non-Actuating Driver Bring-Up**:
在 actuator TBD 期間，僅針對 Provisionally Inhibited Recovery Driver 進行被動電氣完整性、hardware-inhibit readback 與 telemetry observability 的驗證。它不包含 fault injection、powered command 或任何實體 actuator。
_Avoid_: pre-selection driver injection、unloaded output command、live actuator test

**Actuator-Coupled Driver Fault Injection**:
只在 actuator 已選定、可安全代表最終 release path 的測試構型中執行的 Recovery Driver fault injection。actuator TBD 期間，Provisionally Inhibited Recovery Driver 不進行此類注入。
_Avoid_: actuator-free driver fault injection、unqualified release-path test

## Mission integration

**Reserved Payload Envelope**:
為未來任務酬載預留且可被明確量化的結構、質量與隔離的低功耗資料／供電邊界。它不能消耗 recovery、Recovery Authority 或 Flight Computer 的必要功能裕度。
_Avoid_: shared recovery budget、unbounded payload bay、payload-dependent recovery

**Avionics-Owned Payload Budget**:
Flight Computer 先鎖定 Student 3K mission 所需的結構、質量、供電與資料裕度後，才將明確剩餘額度提供給任務酬載。酬載不擁有調整飛行關鍵預算的權限。
_Avoid_: payload-first sizing、negotiated recovery margin

**Payload-Out-of-Band**:
不屬於 Avionics Telemetry v3 contract 的任務酬載資料與狀態，包含 presence、power、health 與原始內容。它不能改變 v3 frame、Common RF Packet Budget 或 Ground Station 對 flight-critical telemetry 的判讀。
_Avoid_: hidden v3 extension、payload health in v3、RF-budget exception

**Payload-Owned Storage**:
由任務酬載自行擁有與管理、且不依賴 Main 或 Backup microSD 的資料保存路徑。它的故障或資料遺失不得改變任何 flight-critical 狀態或 recovery decision。
_Avoid_: shared flight microSD、payload-dependent flight log、avionics storage fallback

**Payload Isolation Boundary**:
將 Reserved Payload Envelope 的供電與資料介面限制為不會反灌、重置、阻塞或觸發 Flight Computer、Recovery Authority 或 recovery path 的單向安全邊界。
_Avoid_: shared avionics rail、payload-to-recovery path、payload-triggered reset

**Armed Payload Power Lock**:
payload 的隔離低功耗電源只在 `SAFE`／`READY` 可設定，進入 `ARMED` 後鎖定至 mission end，且其狀態不構成 flight-critical input。
_Avoid_: in-flight payload reset、RF payload power command、payload-power authority

**Payload Autonomous Self-Restart**:
在 `ARMED` 後，Payload 可在自己的隔離邊界內依其本地狀態執行自主重啟；它不得改變 Armed Payload Power Lock、要求 Flight Computer 或 Ground Station 介入，也不得影響任何 flight-critical 狀態。
_Avoid_: avionics-controlled payload reset、in-flight power reconfiguration、payload-triggered recovery action

**GNSS Pre-Arm Qualification**:
進入 `ARMED` 前 Main GNSS 必須有效，且 Backup 必須完成對共享 GNSS 的 read-only qualification；兩者之一不成立時 ARM request 必須拒絕。這是 launch-time navigation assurance，不改變 GNSS-Optional Recovery Capability。
_Avoid_: Main-only GNSS check、GNSS-warning arm、GNSS-gated in-flight recovery

## Qualification

**Final-Configuration Fault-Injection Evidence**:
在最終 Shared-Enclosure Dual-FC Assembly 中，分別證明每個 Flight Computer 的完整功能，並以受控單點失效情境驗證其故障隔離與 authority 行為的證據。它不能由正常端到端展示、單板測試或純軟體模擬替代。
_Avoid_: happy-path demo、bench-only independence、simulation-only qualification

**Local Inhibit Readback**:
Main 與 Backup 各自對本地 recovery-energy inhibit 的獨立讀取與 v3 記錄。它是可觀測的本地 safety evidence，不由 peer 狀態覆蓋。
_Avoid_: Main-only inhibit status、shared inhibit flag、LED-only evidence

## Ground telemetry

**Independent Ground Transport**:
由單一 RF receiver 與 host serial path 傳入 Ground Station 的 Main 或 Backup telemetry 來源。兩個來源維持分離，並在應用層而非 RF gateway 合併呈現。
_Avoid_: merged radio stream、shared RF gateway

**Avionics Telemetry v3**:
雙飛控的版本化 wire contract，明確表達 Flight Computer identity、boot session、authority 與 recovery evidence。它取代 v2 作為目標架構的 production contract，而 v2 僅保留歷史／相容讀取。
_Avoid_: extended v2、dual-source v2

**Common RF Packet Budget**:
Main 與 Backup 共用的 v3 frame 上限與 transmission rate，依較弱 RF link 的實測 link budget 定義。它不允許任何一方以裁切欄位改變 telemetry 語意。
_Avoid_: per-radio payload variant、best-effort payload

**Frequency-Diverse Backup RF**:
Backup 的 RF 使用與 Main 920–925 MHz E22 path 不同的頻段，且必須先通過 NCC 適用規範、完整 telemetry airtime 與實測 link-budget gate 才能凍結型號及中心頻率。它不預設採用 SX1278/433 MHz。
_Avoid_: unverified 433 fallback、same-band backup

**Competition Acceptance Gate**:
將某一年度台灣盃正式競賽規則、發射許可審查（LRR）要求與當年度書面 RF 條件，連同對應實測證據，轉化為可放行 flight configuration 的外部資格門檻。它不是以前年度規則或自行推定的器材合規。
_Avoid_: historical-rule compliance、assumed competition approval

**Student 3K Mission Target**:
以學生 3K 組為設計的目標任務級別，用來設定高度、回收、RF、供電與驗證裕度；它在當年度正式報名資格與規則公布前，不代表已取得參賽或發射資格。
_Avoid_: approved 2027 class、fixed historical 3K requirements

**Ground-Only Maintenance Link**:
只在 `SAFE`、`READY` 或 Ground Test Session 使用的近距離維護通訊；在進入 `ARMED` 前必須關閉，且不承載 Recovery Authority、recovery command、ARM 或其他飛行關鍵功能。
_Avoid_: flight backup radio、recovery link、in-flight ESP-NOW

**Complete Status Snapshot**:
可獨立判讀的固定長度 v3 binary frame，包含目前的 Main/Backup 身分、各 Flight Computer 感測器最新一筆原始量測及其 timestamp/validity、衍生工程值、健康、flight、authority、channel 與每類 recovery/fault 的最新結果及 sequence。它不攜帶 Payload-Out-of-Band、high-rate sample window、無界歷史、NMEA 或 register dump；完整歷史由連續 snapshot 在地端與機載儲存重建。
_Avoid_: event-only packet、unbounded history packet、payload telemetry frame
