# การทดลองที่ 3: การประมวลผลแบบคู่ขนานพร้อมกลไกการประสานเวลา (Synchronized Concurrency with Mutex)

## 1. วัตถุประสงค์
เพื่อทดสอบและยืนยันประสิทธิภาพของกลไก **Mutual Exclusion (การกีดกันซึ่งกันและกัน)** โดยใช้ **`std::mutex`** และ RAII lock wrapper (**`std::scoped_lock`**) ในการคุ้มกัน Critical Section แบบครอบคลุมทั้งขั้นตอน `CHECK -> DELAY -> UPDATE` เพื่อป้องกันสภาวะการแย่งชิงทรัพยากร (Race Condition) และรับประกันความสอดคล้องของข้อมูล (Data Consistency) ในหน่วยความจำ 100%

---

## 2. ตัวแปรและการตั้งค่าระบบ
| พารามิเตอร์ | ค่าที่กำหนด | คำอธิบาย |
| :--- | :--- | :--- |
| **จำนวน Worker Threads (`--workers`)** | `5` | มี 5 เธรดทำงานคู่ขนานกันจริงในระบบ |
| **กลไก Synchronization (Mutex)** | `ENABLED` | **เปิดใช้งาน Mutex** ล็อกคลุมกระบวนการจองทั้งหมด |
| **การหน่วงเวลาสุ่ม (`--delay`)** | `ENABLED` (50–500 ms) | จำลองโหลดหน่วงเวลาภายใต้การคุ้มครองของ Mutex |
| **เป้าหมายที่นั่ง (Target Seat)** | `A1` | ส่งไคลเอนต์ทุกคนเข้าจองที่นั่งตัวเดียวกัน |
| **จำนวนไคลเอนต์พร้อมกัน (Concurrent Clients)** | `5` (Client 1, 2, 3, 4, 5) | ส่งคำขอเข้าคิว `/css223_reservation_requests` พร้อมกัน |

**คำสั่งที่ใช้รัน Server:**
```bash
./build/debug/src/reservation_server --workers 5 --delay
```

**คำสั่งที่ใช้รัน Client Simulator:**
```bash
./scripts/run_concurrent_clients.sh A1 5 ./build/debug/src/reservation_client
```

---

## 3. บันทึกผลการทดลองจริง (Execution Logs)

### 3.1 บันทึกการทำงานของ Server (`server_exp3.log`)
```text
==============================================================
   CSS223 Cinema Reservation Server Starting                  
==============================================================
  Workers         : 5
  Synchronization : ENABLED
  Random Delay    : ENABLED (50-500 ms)
  Request Queue   : /css223_reservation_requests
==============================================================
[Server] Ready and waiting for client requests. Press Ctrl+C to terminate.
[Worker 1] [CHECK] Seat A1 is AVAILABLE for Client 3
[Worker 1] [DELAY] Simulating random delay: 385 ms for Seat A1
[Worker 1] [UPDATE] Seat A1 SUCCESS: Reserved by Client 3
[Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 4
[Worker 2] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 1] [QUIT] Client 3 session ended
[Worker 2] [UPDATE] Seat A1 reservation FAILED for Client 2
[Worker 3] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 3] [UPDATE] Seat A1 reservation FAILED for Client 1
[Worker 0] [QUIT] Client 4 session ended
[Worker 1] [QUIT] Client 2 session ended
[Worker 4] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 4] [UPDATE] Seat A1 reservation FAILED for Client 5
[Worker 2] [QUIT] Client 1 session ended
[Worker 3] [QUIT] Client 5 session ended
[Server] Shutdown complete. Resources unlinked cleanly.
```

### 3.2 บันทึกการทำงานของไคลเอนต์ (`clients_exp3.log`)
```text
======================================================================
Results Summary for Seat: A1
======================================================================
  Client 1: [FAILED]  Reservation REJECTED
  Client 2: [FAILED]  Reservation REJECTED
  Client 3: [SUCCESS] Reservation GRANTED
  Client 4: [FAILED]  Reservation REJECTED
  Client 5: [FAILED]  Reservation REJECTED
----------------------------------------------------------------------
Total Clients: 5 | Successes: 1 | Failures: 4
>> [OBSERVATION] Exactly ONE reservation succeeded. (Mutual exclusion preserved)
======================================================================
```

### 3.3 ผังที่นั่งในระบบหลังจบการทดลอง (`seat_map_final_exp3.log`)
```text
  +------------------------------------------------------------------+
  |                  ======== CINEMA SCREEN ========                 |
  +------------------------------------------------------------------+

  Row A:  [A1: RSV (C#3)] [A2: AVAIL    ] [A3: AVAIL    ] [A4: AVAIL    ] [A5: AVAIL    ]
  Row B:  [B1: AVAIL    ] [B2: AVAIL    ] [B3: AVAIL    ] [B4: AVAIL    ] [B5: AVAIL    ]
  Row C:  [C1: AVAIL    ] [C2: AVAIL    ] [C3: AVAIL    ] [C4: AVAIL    ] [C5: AVAIL    ]
  Row D:  [D1: AVAIL    ] [D2: AVAIL    ] [D3: AVAIL    ] [D4: AVAIL    ] [D5: AVAIL    ]

  --------------------------------------------------------------------
  Box Office: Total = 20 | Available = 19 | Reserved = 1
  THEATER CAPACITY : [██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░]  5.0%  (1/20 Booked)
  --------------------------------------------------------------------
```

---

## 4. การวิเคราะห์ผลเชิงทฤษฎีระบบปฏิบัติการ (Operating Systems Analysis)
1. **การบังคับใช้คุณสมบัติ Mutual Exclusion สมบูรณ์แบบ:**
   - พิจารณาลำดับการทำงานของ Server:
     ```text
     [Worker 1] [CHECK] Seat A1 is AVAILABLE for Client 3
     [Worker 1] [DELAY] Simulating random delay: 385 ms for Seat A1
     [Worker 1] [UPDATE] Seat A1 SUCCESS: Reserved by Client 3
     ```
   - ในขณะที่ Worker 1 ครอบครอง Mutex lock อยู่นั้น เธรดอื่น (Worker 0, 2, 3, 4) ที่พยายามเข้าสู่ฟังก์ชันการจองจะถูกบล็อกในระดับระบบปฏิบัติการ (Blocked / Sleeping ในคิวของ Mutex)
   - แม้ว่า Worker 1 จะเกิด Delay ยาวนานถึง **385 ms** แต่ไม่มีเธรดอื่นใดสามารถแทรกตัว (Interleave) เข้ามาอ่านสถานะที่นั่งในช่วงเวลานี้ได้เลย
2. **การป้องกันข้อผิดพลาด Time-of-Check to Time-of-Use (TOCTOU):**
   - เมื่อ Worker 1 ดำเนินการจนเสร็จสิ้นและปล่อย Mutex lock เธรดถัดมา (Worker 0) จึงสามารถเข้าสู่ Critical Section ได้
   - เมื่อ Worker 0 เข้าไปตรวจสอบ (`CHECK`) จะพบว่าข้อมูลที่นั่ง A1 ถูกบันทึกเป็น `ALREADY RESERVED (Owner: Client 3)` เรียบร้อยแล้ว จึงปฏิเสธคำขอทันที
3. **การรับประกัน 3 ข้อกำหนดของ Critical-Section Problem:**
   - **Mutual Exclusion:** มีเพียงเธรดเดียวเท่านั้นที่อยู่ใน Critical Section ในเวลาใดเวลาหนึ่ง
   - **Progress:** หากไม่มีเธรดอยู่ใน Critical Section เธรดที่ต้องการเข้าใช้งานสามารถแข่งขันและเข้าใช้งานได้ทันทีโดยไม่เกิด Deadlock
   - **Bounded Waiting:** ด้วยการจัดการคิวของ Mutex ทำให้ทุกเธรดได้รับการประมวลผลจนจบ ไม่มีเธรดใดเกิดสภาวะ Starvation (อดตาย)
