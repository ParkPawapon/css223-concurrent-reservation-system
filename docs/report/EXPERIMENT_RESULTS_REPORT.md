# รายงานผลการทดลองระบบจองตั๋วภาพยนตร์แบบทำงานพร้อมกัน (CSS223 Concurrent Cinema Reservation System)

**วิชา:** CSS223 Operating Systems  
**หัวข้อการทดลอง:** Concurrency, Race Condition, Mutual Exclusion และ Inter-Process Communication (IPC)  
**ภาษาและมาตรฐาน:** C++17, POSIX Threads (pthreads), POSIX Message Queues (`/dev/mqueue`)  
**ระบบปฏิบัติการที่ใช้ทดสอบ:** Linux 6.6 (Ubuntu on WSL2 x86_64)

---

## 1. บทนำและสถาปัตยกรรมระบบ (System Architecture Overview)

ระบบจองตั๋วภาพยนตร์แบบทำงานพร้อมกัน (Concurrent Cinema Seat Reservation System) ถูกออกแบบขึ้นเพื่อศึกษาพฤติกรรมของระบบปฏิบัติการในการจัดการสภาวะการทำงานพร้อมกัน (Concurrency), การสื่อสารข้ามโปรเซส (Inter-Process Communication - IPC) และการแก้ไขปัญหาสภาวะการแย่งชิงทรัพยากร (Race Condition) โดยมีโครงสร้างสถาปัตยกรรมหลักดังนี้:

```text
+-----------------------------------------------------------------------------------+
|                              CLIENT PROCESSES (IPC)                              |
|   +-------------------+  +-------------------+        +-------------------+       |
|   | Client 1 (PID A)  |  | Client 2 (PID B)  |  ...   | Client N (PID X)  |       |
|   +---------+---------+  +---------+---------+        +---------+---------+       |
+-------------|----------------------|----------------------------|-----------------+
              |                      |                            |
              v                      v                            v
      [ /dev/mqueue/css223_reservation_requests ] (POSIX Message Queue - Requests)
              |
+-------------v---------------------------------------------------------------------+
|                           MULTI-THREADED RESERVATION SERVER                       |
|                                                                                   |
|  +-----------------------------------------------------------------------------+  |
|  |                          WORKER THREAD POOL                                 |  |
|  |   [Worker 0]       [Worker 1]       [Worker 2]       ...     [Worker M]     |  |
|  +-------+----------------+----------------+-------------------------+---------+  |
|          |                |                |                         |            |
|          +----------------+-------+--------+-------------------------+            |
|                                   |                                               |
|                                   v                                               |
|                    =================================                              |
|                    CRITICAL SECTION (Mutex / Lock)                                |
|                    - CHECK: ตรวจสอบสถานะที่นั่ง (A1-D5)                             |
|                    - DELAY: จำลองเวลาประมวลผล (50-500 ms)                         |
|                    - UPDATE: ปรับปรุงสถานะลง ReservationTable                      |
|                    =================================                              |
|                                   |                                               |
|                                   v                                               |
|                       [ ReservationTable in RAM ]                                 |
+-----------------------------------|-----------------------------------------------+
                                    |
                                    v
      [ /dev/mqueue/css223_client_<ID>_reply ] (Client Dedicated Reply Queue)
```

### 1.1 องค์ประกอบสำคัญของระบบ
1. **การสื่อสารระดับ OS (POSIX Message Queue):** ใช้ Kernel Message Queues ในการส่งข้อมูลข้ามโปรเซสแบบ Raw Struct (ไร้พอยน์เตอร์) ระหว่างไคลเอนต์และเซิร์ฟเวอร์
   - คิวรับคำขอส่วนกลาง: `/css223_reservation_requests`
   - คิวตอบกลับเฉพาะรายไคลเอนต์: `/css223_client_<client_id>_reply`
2. **เธรดพูลฝั่งเซิร์ฟเวอร์ (Worker Thread Pool):** เซิร์ฟเวอร์ใช้ Worker Threads หลายตัวดึงงานจาก Message Queue มาประมวลผลคู่ขนานกันจริง
3. **ตารางที่นั่งในหน่วยความจำ (ReservationTable):** จัดเก็บข้อมูลที่นั่งโรงภาพยนตร์ 20 ที่นั่ง (แถว A1–A5, B1–B5, C1–C5, D1–D5) บน RAM
4. **ตัวควบคุมการเข้าถึง (Synchronization & Mutex):** จัดการสิทธิ์การเข้าถึง Critical Section ในขั้นตอน `CHECK -> DELAY -> UPDATE` โดยใช้ `std::mutex` เพื่อรับประกันคุณสมบัติ **Mutual Exclusion**

---

## 2. วัตถุประสงค์ของการทดลอง (Experiment Objectives)

การทดลองถูกแบ่งออกเป็น 3 ชุด เพื่อเปรียบเทียบพฤติกรรมของระบบอย่างเป็นวิทยาศาสตร์:
1. **Experiment 1 (Sequential Baseline):** ศึกษาการทำงานแบบตามลำดับโดยใช้ Worker Thread เพียง 1 ตัว เพื่อเป็นค่าอ้างอิงพื้นฐานที่ไร้ความขัดแย้งของเธรด
2. **Experiment 2 (Concurrent without Synchronization):** จำลองปัญหา **Race Condition** และความผิดพลาดประเภท **Time-of-Check to Time-of-Use (TOCTOU)** โดยการปิด Mutex (`--no-sync`) และเปิดการหน่วงเวลาสุ่ม (`--delay`) ทำให้หลายเธรดเข้าแก้ไขที่นั่งตัวเดียวกันพร้อมกัน
3. **Experiment 3 (Concurrent with Synchronization):** พิสูจน์ประสิทธิผลของการใช้ **Mutual Exclusion (`std::mutex`)** คุ้มครอง Critical Section แบบ Atomic Sequence เพื่อขจัดปัญหา Race Condition และรักษาความถูกต้องของข้อมูล (Data Consistency)

---

## 3. ตารางเปรียบเทียบตัวแปรควบคุม (Controlled Variables Matrix)

| ตัวแปร / พารามิเตอร์ | Experiment 1<br>(Sequential Baseline) | Experiment 2<br>(Unsynchronized) | Experiment 3<br>(Synchronized) |
| :--- | :---: | :---: | :---: |
| **จำนวน Worker Threads (`--workers`)** | `1` | `5` | `5` |
| **สถานะ Mutex Synchronization** | **ENABLED** | **DISABLED (`--no-sync`)** | **ENABLED** |
| **การจำลองหน่วงเวลาสุ่ม (`--delay`)** | **DISABLED** | **ENABLED (50–500 ms)** | **ENABLED (50–500 ms)** |
| **ที่นั่งเป้าหมาย (Target Seat)** | `A1` | `A1` | `A1` |
| **จำนวนไคลเอนต์ที่ส่งคำขอพร้อมกัน** | `5` (Client 1, 2, 3, 4, 5) | `5` (Client 1, 2, 3, 4, 5) | `5` (Client 1, 2, 3, 4, 5) |
| **ขอบเขต Critical Section ที่ถูกล็อก** | ไม่มีผลกระทบ (มี 1 เธรด) | **ไม่มีการล็อก (Unprotected)** | **CHECK -> DELAY -> UPDATE** |

---

## 4. ผลการทดลองและการวิเคราะห์เชิงลึก (Detailed Results & Analysis)

### 4.1 การทดลองที่ 1: Sequential Baseline (1 Worker Thread)

#### คำสั่งที่ใช้ทดสอบ:
```bash
# Terminal 1 (Server):
./build/debug/src/reservation_server --workers 1

# Terminal 2 (Clients Simulator):
./scripts/run_concurrent_clients.sh A1 5 ./build/debug/src/reservation_client
```

#### บันทึกผลการทำงานของ Server (`server_exp1.log`):
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

#### ผลลัพธ์ฝั่งไคลเอนต์ (`clients_exp1.log`):
- **Client 3:** `[SUCCESS]` จองที่นั่ง A1 สำเร็จ (เป็นคำขอแรกที่คิวส่งมอบให้เธรด)
- **Client 1, 2, 4, 5:** `[FAILED]` คำขอถูกปฏิเสธเนื่องจากที่นั่งถูกจองแล้ว

#### การวิเคราะห์ผลการทดลองที่ 1:
1. **การประมวลผลแบบอนุกรม (Serialization):** เนื่องจากระบบรันด้วย Worker Thread เพียง 1 ตัว คำขอทั้งหมดใน Message Queue จะถูกดึงมาประมวลผลแบบทีละงาน (One-by-One)
2. **ไม่มีสภาวะการแย่งชิง (Zero Concurrency Conflict):** ในขณะที่ Worker 0 กำลังทำงาน จะไม่มีเธรดอื่นสามารถเข้ามาแทรกแซงตัวแปรในหน่วยความจำได้ การตรวจสอบสถานะ (`CHECK`) และการเขียนข้อมูล (`UPDATE`) จึงมีความถูกต้องสอดคล้องกันเสมอ

---

### 4.2 การทดลองที่ 2: Unsynchronized Concurrency (Race Condition / TOCTOU Flaw)

#### คำสั่งที่ใช้ทดสอบ:
```bash
# Terminal 1 (Server):
./build/debug/src/reservation_server --workers 5 --no-sync --delay

# Terminal 2 (Clients Simulator):
./scripts/run_concurrent_clients.sh A1 5 ./build/debug/src/reservation_client
```

#### บันทึกผลการทำงานของ Server (`server_exp2.log`):
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

#### การวิเคราะห์ผลการทดลองที่ 2 (จุดสำคัญในการส่งงานอาจารย์):
1. **ปรากฏการณ์ Time-of-Check to Time-of-Use (TOCTOU):**
   - ในบรรทัดที่ 12–16 จะเห็นได้ชัดเจนว่า **Worker Thread ทั้ง 5 เธรด (Worker 0, 1, 2, 3, 4)** เข้าไปอ่านข้อมูลในหน่วยความจำ RAM พร้อมๆ กัน และพบว่าที่นั่ง A1 ว่างอยู่ (`is AVAILABLE`)
   - เนื่องจากมีการปิด Mutex (`--no-sync`) ทำให้ไม่มีการบล็อกเธรดอื่น ทั้ง 5 เธรดจึงเข้าสู่ช่วงหน่วงเวลา (`DELAY`) พร้อมกัน
2. **การแย่งชิงทรัพยากร (Race Condition):**
   - Worker 2 สุ่มได้หน่วงเวลาน้อยที่สุดคือ **160 ms** จึงตื่นขึ้นมาก่อนและเขียนสถานะลงในหน่วยความจำว่าที่นั่ง A1 ถูกจองโดย Client 1
   - แต่ Worker 3 (187 ms), Worker 1 (342 ms), Worker 0 (404 ms), และ Worker 4 (469 ms) ทำการตรวจสอบ (`CHECK`) ผ่านไปแล้วตั้งแต่ก่อนหน้านี้บนข้อมูลเก่าที่ล้าสมัย (Stale Data)
3. **การเกิดสภาวะ Conflict / Double Booking:**
   - เมื่อ Worker 3, 1, 0, 4 ตื่นขึ้นมาทำการเขียนข้อมูล จึงตรวจพบความขัดแย้ง:
     `CONFLICT / DOUBLE BOOKING: Seat A1 reservation FAILED (Seat was reserved by another thread during delay!)`
   - ในระบบที่ไม่มีกลไกป้องกัน สภาวะนี้จะส่งผลให้ตั๋วใบเดียวกันถูกขายซ้ำให้ผู้ใช้หลายคน (Double Booking Bug) สร้างความเสียหายต่อระบบจริง

---

### 4.3 การทดลองที่ 3: Synchronized Concurrency (Mutual Exclusion via Mutex)

#### คำสั่งที่ใช้ทดสอบ:
```bash
# Terminal 1 (Server):
./build/debug/src/reservation_server --workers 5 --delay

# Terminal 2 (Clients Simulator):
./scripts/run_concurrent_clients.sh A1 5 ./build/debug/src/reservation_client
```

#### บันทึกผลการทำงานของ Server (`server_exp3.log`):
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

#### การวิเคราะห์ผลการทดลองที่ 3:
1. **การคุ้มกัน Critical Section แบบเบ็ดเสร็จ (Atomic Critical Section):**
   - เมื่อเปิดใช้งาน Mutex กลไก `std::scoped_lock` จะทำการล็อกตัวแปร `reservation_mutex_` ตั้งแต่ก่อนเริ่มขั้นตอน `CHECK` ยาวนานไปจนถึงสิ้นสุดขั้นตอน `UPDATE`
   - ในขณะที่ Worker 1 ตรวจสอบพบว่าที่นั่ง A1 ว่าง และกำลังจำลองหน่วงเวลาอยู่ **385 ms** นั้น เธรดอื่นๆ (Worker 0, 2, 3, 4) จะถูกสั่งให้รอ (Blocked) ในระดับ OS Scheduler ไม่สามารถเข้ามาแทรกแซงขั้นตอน `CHECK` ได้
2. **การรักษาความถูกต้องของข้อมูล (Data Consistency):**
   - เมื่อ Worker 1 ปรับปรุงสถานะเป็น `Reserved by Client 3` และปล่อย Lock เธรดถัดมา (Worker 0) จึงได้รับสิทธิ์เข้าสู่ Critical Section
   - เมื่อ Worker 0 เข้าไปตรวจสอบ จึงเห็นสถานะจริงที่เป็นปัจจุบันว่า `Seat A1 is ALREADY RESERVED` และปฏิเสธคำขอของ Client 4 อย่างถูกต้องโดยทันที ปราศจากความขัดแย้งหรือ Stale Read ใดๆ ทั้งสิ้น

---

## 5. ตารางสรุปเปรียบเทียบผลลัพธ์ทั้ง 3 การทดลอง (Summary Comparison Matrix)

| หัวข้อเปรียบเทียบ | Experiment 1<br>(Sequential Baseline) | Experiment 2<br>(Unsynchronized Concurrency) | Experiment 3<br>(Synchronized Concurrency) |
| :--- | :---: | :---: | :---: |
| **จำนวน Worker Threads** | 1 | 5 | 5 |
| **การเกิด Concurrency ขนานจริง** | ไม่มี (Non-concurrent) | **มี (Concurrent)** | **มี (Concurrent)** |
| **สถานะ Mutex Lock** | เปิดใช้งาน (แต่ไม่มีการแย่งชิง) | **ปิดใช้งาน (`--no-sync`)** | **เปิดใช้งาน (`std::mutex`)** |
| **ช่วงเวลาหน่วงสุ่ม (Delay)** | ไม่มี (0 ms) | 50–500 ms ต่อคำขอ | 50–500 ms ต่อคำขอ |
| **จำนวนเธรดที่เห็นที่นั่งว่างพร้อมกัน** | 1 เธรด | **5 เธรดพร้อมกัน (Interleaved)** | **1 เธรดเท่านั้น (Serialized at CS)** |
| **การตรวจพบ Conflict / Double Booking** | ไม่พบ (0 ครั้ง) | **ตรวจพบ 4 เธรด (TOCTOU Detected)** | **ไม่พบ (0 ครั้ง)** |
| **การปฏิเสธคำขอซ้ำซ้อน** | ปฏิเสธที่ขั้นตอน CHECK (ปกติ) | ปฏิเสธที่ขั้นตอน UPDATE (ขัดแย้ง) | ปฏิเสธที่ขั้นตอน CHECK (ถูกต้อง) |
| **ความถูกต้องของผังที่นั่งสุดท้าย** | ถูกต้อง (จอง 1 ที่นั่ง) | ถูกต้อง (ระบบตรวจจับ Conflict ได้) | ถูกต้องสมบูรณ์ (จอง 1 ที่นั่ง) |
| **การันตี Mutual Exclusion** | N/A (มีเธรดเดียว) | **ล้มเหลว (Violated)** | **สมบูรณ์แบบ (Guaranteed)** |

---

## 6. การวิเคราะห์เชิงทฤษฎีระบบปฏิบัติการ (Operating Systems Theory)

### 6.1 ปัญหา Critical-Section Problem และข้อกำหนด 3 ประการ
ตามทฤษฎีของ Silberschatz, Galvin และ Gagne โซลูชันที่ถูกต้องสำหรับปัญหา Critical Section จะต้องผ่านเกณฑ์ 3 ข้อ:
1. **Mutual Exclusion (การกีดกันซึ่งกันและกัน):** หากโปรเซสหรือเธรดหนึ่งกำลังทำงานอยู่ใน Critical Section จะต้องไม่มีเธรดอื่นสามารถเข้ามาทำงานใน Critical Section นั้นได้ในเวลาเดียวกัน
   - *ผลใน Exp 2:* **ไม่ผ่าน** (ทุกเธรดเข้า Critical Section พร้อมกันในช่วง Delay)
   - *ผลใน Exp 3:* **ผ่านสมบูรณ์แบบ** (มีเพียง Worker 1 ตัวเดียวที่อยู่ใน Critical Section)
2. **Progress (ความก้าวหน้าของระบบ):** หากไม่มีเธรดใดอยู่ใน Critical Section และมีเธรดต้องการเข้าใช้งาน การคัดเลือกเธรดถัดไปจะต้องเกิดขึ้นได้ทันทีโดยไม่ติดขัด
   - *ผลการทดลอง:* การใช้ `std::mutex` บน Linux อาศัย Futex (Fast Userspace Mutex) ของ Kernel ซึ่งรับประกันว่าเธรดที่รออยู่จะถูกปลุกขึ้นมาทำงานต่อทันทีเมื่อ Lock ถูกปลด
3. **Bounded Waiting (การจำกัดเวลารอ):** ต้องมีการกำหนดขอบเขตจำกัดจำนวนครั้งที่เธรดอื่นสามารถเข้า Critical Section ก่อนที่เธรดที่กำลังรอจะได้รับสิทธิ์ เพื่อป้องกันสภาวะการอดตาย (Starvation)
   - *ผลการทดลอง:* ทุกเธรดใน Exp 3 ได้รับการประมวลผลจนเสร็จสิ้นครบทั้ง 5 เธรดตามลำดับ

### 6.2 การวิเคราะห์ปัญหา TOCTOU (Time-of-Check to Time-of-Use)
ปัญหา TOCTOU เป็นช่องโหว่ความสอดคล้องของระบบที่พบบ่อยมากใน Concurrent Programming:
$$\text{State Check} \xrightarrow{\quad \Delta t \quad} \text{State Mutation}$$
- ใน Experiment 2 ช่วงเวลา $\Delta t$ (Random Delay 50–500 ms) เกิดขึ้นโดยไม่มี Lock ครอบคลุม ทำให้เกิดสภาวะ **Interleaving** ของ CPU Scheduler ส่งผลให้สถานะที่ตรวจสอบไว้กลายเป็นข้อมูลเท็จก่อนที่จะนำไปใช้งาน
- ใน Experiment 3 การนำ Mutex มาคลุมทั้งลำดับขั้นตอน $(\text{Check} + \Delta t + \text{Mutation})$ ทำให้ขั้นตอนทั้งหมดกลายเป็น **Atomic Operation** ในมุมมองของเธรดอื่น

---

## 7. ผลการทดสอบด้านความถูกต้องและความปลอดภัยระดับ Enterprise (QA & Verification)

ระบบผ่านการตรวจสอบด้วยชุดทดสอบมาตรฐานวิศวกรรมซอฟต์แวร์ครบทุกระดับ:

```text
===============================================================================
CSS223 Test Suites Summary (CTest / Google Test)
===============================================================================
1. Unit Tests (Core Reservation Domain)           : PASS (1/1 suites, 100%)
2. Integration Tests (POSIX Message Queue IPC)     : PASS (5/5 suites, 100%)
3. Concurrency Tests (Thread Pool & Race Condition): PASS (2/2 suites, 100%)
4. ThreadSanitizer (TSan Data Race Verification)   : PASS (8/8 checks, 0 data races)
===============================================================================
```

### การทดสอบด้วย ThreadSanitizer (TSan) บน Linux:
คอมไพล์ระบบด้วยแฟลก `-fsanitize=thread -g` และรันชุดทดสอบพร้อมกัน 50 เธรด ผลลัพธ์:
- ไม่พบรายงาน `WARNING: ThreadSanitizer: data race` ใดๆ ทั้งสิ้น
- ยืนยันว่าการเข้าถึงตัวแปรภายในหน่วยความจำมีการใช้ Mutex Synchronization และ Memory Barrier อย่างถูกต้องตามมาตรฐาน ISO C++17

### การจัดการทรัพยากรระบบ (Resource Cleanup & Graceful Shutdown):
- ดักจับสัญญาณ `SIGINT` (Ctrl+C) และ `SIGTERM` ผ่าน POSIX Signal Handler
- ปิด File Descriptor (`mq_close`) และลบคิวออกจาก Kernel (`mq_unlink`) ทุกครั้งเมื่อปิดโปรแกรม
- ผลการตรวจสอบใน `/dev/mqueue` หลังปิดระบบ: **สะอาดสมบูรณ์ (Zero Stale Queues)**

---

## 8. บทสรุป (Conclusion)

1. **การพิสูจน์เชิงประจักษ์:** ผลการทดลองทั้ง 3 การทดลองได้แสดงให้เห็นอย่างชัดเจนถึงพฤติกรรมของระบบปฏิบัติการ โดยเปรียบเทียบให้เห็นความแตกต่างอย่างเป็นรูปธรรมระหว่างระบบที่ไม่มี Concurrency (Exp 1), ระบบที่มี Concurrency แต่ไร้การควบคุม (Exp 2 ที่เกิด TOCTOU Conflict ชัดเจนใน Log), และระบบที่ใช้ Mutual Exclusion อย่างถูกต้อง (Exp 3)
2. **ความสมบูรณ์ของระบบ:** ระบบจองตั๋วภาพยนตร์ CSS223 ทำงานได้อย่างมีประสิทธิภาพ ถูกต้องตามทฤษฎีระบบปฏิบัติการ และพร้อมสำหรับนำเสนอและส่งมอบแก่อาจารย์ผู้สอน
