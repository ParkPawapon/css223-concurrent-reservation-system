# การทดลองที่ 2: สภาวะการแย่งชิงทรัพยากรเมื่อไม่มีการประสานเวลา (Race Condition / Unsynchronized Concurrency)

## 1. วัตถุประสงค์
เพื่อจำลองและพิสูจน์ปัญหา **Race Condition** และข้อผิดพลาดประเภท **Time-of-Check to Time-of-Use (TOCTOU)** ที่เกิดขึ้นในระดับระบบปฏิบัติการ เมื่อมีหลาย Worker Threads เข้าถึงและแก้ไข Critical Section ในหน่วยความจำพร้อมกัน โดยจงใจ **ปิดการทำงานของ Mutex (`--no-sync`)** และเปิดการจำลองความล่าช้าของ I/O หรือการประมวลผล (**Random Delay 50–500 ms**)

---

## 2. ตัวแปรและการตั้งค่าระบบ
| พารามิเตอร์ | ค่าที่กำหนด | คำอธิบาย |
| :--- | :--- | :--- |
| **จำนวน Worker Threads (`--workers`)** | `5` | มี 5 เธรดทำงานคู่ขนานกันจริงในระบบ |
| **กลไก Synchronization (Mutex)** | `DISABLED` (`--no-sync`) | **จงใจปิดการล็อก Mutex** ในช่วง Critical Section |
| **การหน่วงเวลาสุ่ม (`--delay`)** | `ENABLED` (50–500 ms) | จำลองการประมวลผลหรือโหลดเครือข่าย เพื่อเปิดหน้าต่างเวลาแย่งชิง |
| **เป้าหมายที่นั่ง (Target Seat)** | `A1` | ส่งไคลเอนต์ทุกคนเข้าจองที่นั่งตัวเดียวกัน |
| **จำนวนไคลเอนต์พร้อมกัน (Concurrent Clients)** | `5` (Client 1, 2, 3, 4, 5) | ส่งคำขอเข้าคิว `/css223_reservation_requests` พร้อมกัน |

**คำสั่งที่ใช้รัน Server:**
```bash
./build/debug/src/reservation_server --workers 5 --no-sync --delay
```

**คำสั่งที่ใช้รัน Client Simulator:**
```bash
./scripts/run_concurrent_clients.sh A1 5 ./build/debug/src/reservation_client
```

---

## 3. บันทึกผลการทดลองจริง (Execution Logs)

### 3.1 บันทึกการทำงานของ Server (`server_exp2.log`)
```text
==============================================================
   CSS223 Cinema Reservation Server Starting                  
==============================================================
  Workers         : 5
  Synchronization : DISABLED (--no-sync)
  Random Delay    : ENABLED (50-500 ms)
  Request Queue   : /css223_reservation_requests
==============================================================
[Server] Ready and waiting for client requests. Press Ctrl+C to terminate.
[Worker 1] [CHECK] Seat A1 is AVAILABLE for Client 3
[Worker 2] [CHECK] Seat A1 is AVAILABLE for Client 1
[Worker 3] [CHECK] Seat A1 is AVAILABLE for Client 2
[Worker 0] [CHECK] Seat A1 is AVAILABLE for Client 5
[Worker 4] [CHECK] Seat A1 is AVAILABLE for Client 4
[Worker 2] [DELAY] Simulating random delay: 160 ms for Seat A1
[Worker 2] [UPDATE] Seat A1 SUCCESS: Reserved by Client 1
[Worker 2] [QUIT] Client 1 session ended
[Worker 3] [DELAY] Simulating random delay: 187 ms for Seat A1
[Worker 3] [UPDATE] CONFLICT / DOUBLE BOOKING: Seat A1 reservation FAILED for Client 2 (Seat was reserved by another thread during delay!)
[Worker 2] [QUIT] Client 2 session ended
[Worker 1] [DELAY] Simulating random delay: 342 ms for Seat A1
[Worker 1] [UPDATE] CONFLICT / DOUBLE BOOKING: Seat A1 reservation FAILED for Client 3 (Seat was reserved by another thread during delay!)
[Worker 3] [QUIT] Client 3 session ended
[Worker 0] [DELAY] Simulating random delay: 404 ms for Seat A1
[Worker 0] [UPDATE] CONFLICT / DOUBLE BOOKING: Seat A1 reservation FAILED for Client 5 (Seat was reserved by another thread during delay!)
[Worker 2] [QUIT] Client 5 session ended
[Worker 4] [DELAY] Simulating random delay: 469 ms for Seat A1
[Worker 4] [UPDATE] CONFLICT / DOUBLE BOOKING: Seat A1 reservation FAILED for Client 4 (Seat was reserved by another thread during delay!)
[Worker 1] [QUIT] Client 4 session ended
[Server] Shutdown complete. Resources unlinked cleanly.
```

### 3.2 บันทึกการทำงานของไคลเอนต์ (`clients_exp2.log`)
```text
======================================================================
Results Summary for Seat: A1
======================================================================
  Client 1: [SUCCESS] Reservation GRANTED
  Client 2: [FAILED]  Reservation REJECTED
  Client 3: [FAILED]  Reservation REJECTED
  Client 4: [FAILED]  Reservation REJECTED
  Client 5: [FAILED]  Reservation REJECTED
----------------------------------------------------------------------
Total Clients: 5 | Successes: 1 | Failures: 4
>> [OBSERVATION] Exactly ONE reservation succeeded. (Mutual exclusion preserved)
======================================================================
```

### 3.3 ผังที่นั่งในระบบหลังจบการทดลอง (`seat_map_final_exp2.log`)
```text
  +------------------------------------------------------------------+
  |                  ======== CINEMA SCREEN ========                 |
  +------------------------------------------------------------------+

  Row A:  [A1: RSV (C#1)] [A2: AVAIL    ] [A3: AVAIL    ] [A4: AVAIL    ] [A5: AVAIL    ]
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
1. **การเกิด Race Condition & Time-of-Check to Time-of-Use (TOCTOU):**
   - ดูจากบันทึกของ Server ในช่วงบรรทัดที่ 12–16:
     ```text
     [Worker 1] [CHECK] Seat A1 is AVAILABLE for Client 3
     [Worker 2] [CHECK] Seat A1 is AVAILABLE for Client 1
     [Worker 3] [CHECK] Seat A1 is AVAILABLE for Client 2
     [Worker 0] [CHECK] Seat A1 is AVAILABLE for Client 5
     [Worker 4] [CHECK] Seat A1 is AVAILABLE for Client 4
     ```
   - **ปรากฏการณ์ที่เกิดขึ้น:** Worker Thread ทั้ง 5 ตัวเข้าสู่ฟังก์ชันตรวจสอบสถานะที่นั่งเกือบพร้อมกัน ทั้ง 5 เธรดอ่านสถานะของที่นั่ง A1 จากหน่วยความจำ RAM และต่างสรุปตรงกันว่า **"Seat A1 is AVAILABLE"**
2. **หน้าต่างแห่งความขัดแย้ง (Interleaved Execution Window):**
   - เมื่อไม่มี Mutex คอยล็อกคุ้มกัน Critical Section แต่ละเธรดจะแยกย้ายกันเข้าสู่ขั้นตอนหน่วงเวลา (Random Delay):
     - Worker 2 สุ่มได้ระยะเวลาหน่วงเวลาสั้นที่สุดคือ **160 ms** จึงตื่นขึ้นมาก่อนและทำการเขียนข้อมูลลงหน่วยความจำ (`[UPDATE] Seat A1 SUCCESS: Reserved by Client 1`)
     - Worker 3 สุ่มได้หน่วงเวลา **187 ms** เมื่อตื่นขึ้นมาจะพยายามจองที่นั่ง A1 โดยอ้างอิงจากการตรวจสอบก่อนหน้าว่าที่นั่งว่าง แต่ ณ เวลานั้นหน่วยความจำถูก Worker 2 เขียนทับไปแล้ว
3. **การตรวจจับ Conflict / Double Booking:**
   - สถาปัตยกรรมของเซิร์ฟเวอร์ได้รับการออกแบบให้ตรวจจับความขัดแย้ง ณ จังหวะเขียน (`UPDATE`) จึงสามารถตรวจพบได้ว่าสถานะใน RAM เปลี่ยนแปลงไปจากจังหวะที่ `CHECK` ไว้:
     ```text
     [Worker 3] [UPDATE] CONFLICT / DOUBLE BOOKING: Seat A1 reservation FAILED for Client 2 (Seat was reserved by another thread during delay!)
     ```
   - หากระบบไม่ได้เขียนโค้ดตรวจสอบซ้ำซ้อนในระดับโปรแกรม หรือหากเป็นการเขียนทับดิบๆ (Raw Unsynchronized Write) จะเกิดเหตุการณ์ **Double Booking ในฐานข้อมูลจริง** ซึ่งเป็นความเสียหายร้ายแรงในระบบ Distributed / Concurrent Enterprise Systems
