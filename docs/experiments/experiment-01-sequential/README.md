# การทดลองที่ 1: การประมวลผลแบบตามลำดับ (Sequential Baseline)

## 1. วัตถุประสงค์
เพื่อเป็นชุดข้อมูลพื้นฐานอ้างอิง (Baseline) ในการทดสอบระบบ โดยบังคับให้ระบบมี Worker Thread เพียง 1 เธรด ทำหน้าที่ประมวลผลคำขอจาก Message Queue ทีละคำขอตามลำดับก่อนหลัง (First-In, First-Out: FIFO) เพื่อยืนยันว่าระบบทำงานได้อย่างถูกต้องสมบูรณ์เมื่อไม่มีสภาวะการแย่งชิงทรัพยากร (Concurrency) เข้ามาเกี่ยวข้อง

---

## 2. ตัวแปรและการตั้งค่าระบบ
| พารามิเตอร์ | ค่าที่กำหนด | คำอธิบาย |
| :--- | :--- | :--- |
| **จำนวน Worker Threads (`--workers`)** | `1` | จำกัดเธรดประมวลผลเพียง 1 เธรด |
| **กลไก Synchronization (Mutex)** | `ENABLED` | เปิดใช้งานตัวควบคุมความปลอดภัยตามปกติ |
| **การหน่วงเวลาสุ่ม (`--delay`)** | `DISABLED` | ประมวลผลคำขอทันที ไม่มีการจำลองโหลดหน่วงเวลา |
| **เป้าหมายที่นั่ง (Target Seat)** | `A1` | ส่งไคลเอนต์ทุกคนเข้าจองที่นั่งตัวเดียวกัน |
| **จำนวนไคลเอนต์พร้อมกัน (Concurrent Clients)** | `5` (Client 1, 2, 3, 4, 5) | ส่งคำขอเข้าคิว `/css223_reservation_requests` |

**คำสั่งที่ใช้รัน Server:**
```bash
./build/debug/src/reservation_server --workers 1
```

**คำสั่งที่ใช้รัน Client Simulator:**
```bash
./scripts/run_concurrent_clients.sh A1 5 ./build/debug/src/reservation_client
```

---

## 3. บันทึกผลการทดลองจริง (Execution Logs)

### 3.1 บันทึกการทำงานของ Server (`server_exp1.log`)
```text
==============================================================
   CSS223 Cinema Reservation Server Starting                  
==============================================================
  Workers         : 1
  Synchronization : ENABLED
  Random Delay    : DISABLED
  Request Queue   : /css223_reservation_requests
==============================================================
[Server] Ready and waiting for client requests. Press Ctrl+C to terminate.
[Worker 0] [CHECK] Seat A1 is AVAILABLE for Client 3
[Worker 0] [UPDATE] Seat A1 SUCCESS: Reserved by Client 3
[Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 5
[Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 1
[Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 4
[Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
[Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 2
[Worker 0] [QUIT] Client 3 session ended
[Worker 0] [QUIT] Client 5 session ended
[Worker 0] [QUIT] Client 4 session ended
[Worker 0] [QUIT] Client 1 session ended
[Worker 0] [QUIT] Client 2 session ended
[Server] Shutdown complete. Resources unlinked cleanly.
```

### 3.2 บันทึกการทำงานของไคลเอนต์ (`clients_exp1.log`)
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

### 3.3 ผังที่นั่งในระบบหลังจบการทดลอง (`seat_map_final_exp1.log`)
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
1. **การเรียงลำดับแบบอนุกรม (Serialization by Design):** แม้ว่าไคลเอนต์ทั้ง 5 ตัวจะส่งคำขอเข้ามายัง POSIX Message Queue เกือบจะพร้อมๆ กัน แต่คิวจะจัดเก็บข้อความตามลำดับเวลาที่มาถึง (FIFO) เมื่อเซิร์ฟเวอร์รันด้วย Worker Thread เพียง 1 เธรด เธรดดังกล่าวจะทำการ `mq_receive()` คำขอที่ 1 (ในที่นี้คือ Client 3) เข้าไปทำงาน ตรวจสอบสถานะ (`CHECK`) พบว่าว่าง และอัปเดต (`UPDATE`) ให้เป็นของ Client 3 ทันที
2. **การป้องกัน Race Condition โดยธรรมชาติของ Single-Threaded Core:** ในสภาวะที่มี 1 Worker Thread จะไม่มีทางเกิด Race Condition หรือ Data Race บนตัวแปรในหน่วยความจำได้เลย เพราะไม่มี Context Switch ข้ามเธรดระหว่างการดำเนินการใน Critical Section
3. **ผลลัพธ์ของคำขอถัดมา:** เมื่อคำขอที่ 2 (Client 5), 3 (Client 1), 4 (Client 4), และ 5 (Client 2) ถูกดึงออกมาประมวลผลตามลำดับ ขั้นตอน `CHECK` จะอ่านสถานะล่าสุดพบว่าเป็น `RESERVED` โดยมีเจ้าของคือ Client 3 อยู่แล้ว จึงส่งผลให้คำขอที่เหลือถูกปฏิเสธ (REJECTED) อย่างถูกต้อง 100%
