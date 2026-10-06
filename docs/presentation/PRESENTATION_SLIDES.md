# สไลด์นำเสนอโครงงาน: Concurrent Cinema Seat Reservation System
**วิชา:** CSS223 Operating Systems  
**หัวข้อ:** การศึกษา Concurrency, Race Condition, Mutual Exclusion และ POSIX Message Queue  
**ภาษาและสภาพแวดล้อม:** C++17 บน Linux Kernel (POSIX Threads & `<mqueue.h>`)

---

## สไลด์ที่ 1: หน้าปกโครงงาน (Title Slide)

### เนื้อหาบนสไลด์:
- **ชื่อโครงงาน:** CSS223 Concurrent Cinema Seat Reservation System
- **หัวข้อศึกษา:** การจัดการ Concurrency, ปัญหา Race Condition และการสื่อสารข้ามโปรเซสด้วย POSIX Message Queue
- **วิชา:** CSS223 Operating Systems
- **เทคโนโลยีที่ใช้:** C++17, POSIX Threads (pthreads), POSIX Message Queue (`/dev/mqueue`), Linux 6.6 Kernel
- **สมาชิกในกลุ่ม:** [ใส่ชื่อ-รหัสนักศึกษาของสมาชิก]

### สิ่งที่ควรแสดงบนสไลด์ (Visual):
- โลโก้ภาควิชา/มหาวิทยาลัย
- ภาพสถาปัตยกรรมย่อ (Client Processes $\leftrightarrow$ Message Queue $\leftrightarrow$ Multi-threaded Server)

### บทพูดผู้นำเสนอ (Speaker Notes):
> "สวัสดีครับอาจารย์และเพื่อนๆ ทุกคน วันนี้กลุ่มพวกผมจะมานำเสนอโครงงานระบบสำรองที่นั่งโรงภาพยนตร์แบบทำงานร่วมกัน หรือ Concurrent Cinema Seat Reservation System ซึ่งพัฒนาด้วยภาษา C++17 บนระบบปฏิบัติการ Linux ครับ  
> โครงงานนี้มีเป้าหมายหลักเพื่อศึกษาการทำงานของ Operating System Primitives โดยเน้นไปที่การสื่อสารข้ามโปรเซส (IPC) ผ่าน POSIX Message Queue และการแก้ปัญหาสภาวะการแย่งชิงทรัพยากร (Race Condition) ในระบบ Multi-threading ครับ"

---

## สไลด์ที่ 2: ปัญหาและที่มาของโครงงาน (Problem Statement & Motivation)

### เนื้อหาบนสไลด์:
- **โจทย์ในโลกจริง (Real-World Scenario):**
  - เมื่อมีผู้ใช้หลายคนกดจองตั๋วภาพยนตร์ที่นั่งเดียวกันพร้อมกันในเสี้ยววินาที
  - หากระบบจัดการไม่ดี จะเกิดปัญหา **การขายตั๋วซ้ำ (Double Booking)**
- **ปัญหาทางด้านระบบปฏิบัติการ (OS Challenges):**
  - **Shared Memory Concurrency:** หลายเธรดเข้าถึงตารางที่นั่งในหน่วยความจำ RAM พร้อมกัน
  - **Preemption & Scheduling:** ระบบปฏิบัติการอาจสลับการทำงานของเธรด (Context Switch) ในจังหวะที่ข้อมูลยังบันทึกไม่เสร็จ
  - **Time-of-Check to Time-of-Use (TOCTOU):** ข้อมูลที่นั่งที่อ่านไว้ตอนแรก อาจถูกเธรดอื่นแก้ไขไปแล้วก่อนที่เราจะทันได้เขียนบันทึก

### สิ่งที่ควรแสดงบนสไลด์ (Visual):
- แผนภาพเปรียบเทียบ: Client A และ Client B จองที่นั่ง A1 พร้อมกัน $\rightarrow$ แย่งกันเขียน $\rightarrow$ ระบบบันทึกซ้ำซ้อน

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ปัญหาคลาสสิกของระบบจองตั๋วคือ เมื่อมีผู้ใช้จำนวนมากส่งคำขอเข้ามาจองที่นั่งตัวเดียวกันพร้อมกัน ถ้าเซิร์ฟเวอร์ทำงานแบบมัลติเธรดโดยไม่มีการควบคุมจังหวะเวลา เธรดแต่ละตัวจะอ่านข้อมูลจาก RAM ในเวลาเดียวกัน แล้วต่างคนต่างสรุปว่าที่นั่งว่าง  
> เมื่อเกิด Context Switch หรือมีความล่าช้าในการประมวลผล เธรดตัวหนึ่งจะเขียนบันทึกไปก่อน ในขณะที่เธรดตัวอื่นที่อ่านข้อมูลเก่าค้างไว้ก็จะเขียนทับซ้ำลงไป เกิดเป็นบั๊ก Double Booking ซึ่งในทาง OS เราเรียกปัญหานี้ว่า Time-of-Check to Time-of-Use หรือ TOCTOU ครับ"

---

## สไลด์ที่ 3: ทฤษฎีระบบปฏิบัติการที่นำมาใช้ (OS Theoretical Foundations)

### เนื้อหาบนสไลด์:
1. **Inter-Process Communication (IPC):**
   - ใช้ **POSIX Message Queue (`/dev/mqueue`)** ซึ่งจัดการโดย Kernel
   - ข้อมูลส่งเป็น Message Record ขนาดคงที่ (Raw Trivially Copyable Struct) ปลอดภัยข้ามโปรเซส
2. **Worker Thread Pool:**
   - เซิร์ฟเวอร์สร้างชุดเธรดคนงานล่วงหน้า เพื่อรอรับคำขอจาก Message Queue ไปประมวลผลคู่ขนานจริง
3. **Critical-Section Problem:**
   - โค้ดส่วนที่เข้าถึงและแก้ไขตารางที่นั่งใน RAM ถือเป็น **Critical Section**
   - โซลูชันต้องผ่านเกณฑ์ 3 ข้อของ Silberschatz:
     1. **Mutual Exclusion:** ต้องมีเธรดเดียวเท่านั้นใน Critical Section
     2. **Progress:** ถ้าไม่มีใครอยู่ คนที่รอต้องเข้าไปทำงานได้ทันที
     3. **Bounded Waiting:** ต้องไม่มีเธรดใดต้องรอนานจนเกิดสภาวะอดตาย (Starvation)
4. **Mutex Synchronization:**
   - ใช้ `std::mutex` และ RAII Wrapper (`std::scoped_lock`) คุ้มครองลำดับขั้นตอนให้เป็น Atomic Operation

### สิ่งที่ควรแสดงบนสไลด์ (Visual):
- กล่อง Critical Section 3 ขั้นตอน: `[ CHECK ] -> [ RANDOM DELAY ] -> [ UPDATE ]` ครอบด้วยแม่กุญแจ `std::mutex`

### บทพูดผู้นำเสนอ (Speaker Notes):
> "เพื่อให้เข้าใจการแก้ปัญหา เราอิงตามทฤษฎีระบบปฏิบัติการหลักๆ 3 เรื่องครับ  
> เรื่องแรกคือ IPC เราเลือกใช้ POSIX Message Queue ของ Linux ในการส่งรับข้อความแบบมีขอบเขตและปลอดภัย  
> เรื่องที่สองคือการจัดการเธรด เราใช้ Worker Thread Pool เพื่อรองรับคำขอพร้อมกัน  
> และเรื่องที่สามคือ Critical-Section Problem ซึ่งขั้นตอนการจองที่นั่งประกอบด้วย 3 จังหวะคือ ตรวจสอบสถานะ (CHECK), จำลองเวลาประมวลผล (DELAY), และบันทึกผล (UPDATE)  
> เราต้องทำให้ 3 ขั้นตอนนี้ทำงานเสร็จสิ้นแบบแยกขาดจากกันไม่ได้ (Atomic Sequence) ด้วยการล็อก Mutex เพื่อรับประกันคุณสมบัติ Mutual Exclusion ครับ"

---

## สไลด์ที่ 4: สถาปัตยกรรมระบบ (System Architecture)

### เนื้อหาบนสไลด์:
```text
+---------------------+       +---------------------+
|  Client 1 Process   |  ...  |  Client N Process   |
+----------+----------+       +----------+----------+
           |                             |
           +--------------+--------------+
                          | (mq_send)
                          v
         [/dev/mqueue/css223_reservation_requests]
                          |
                          | (mq_receive)
                          v
        +-----------------------------------+
        | Multi-Threaded Reservation Server |
        |   - Worker Pool (M Threads)       |
        |   - std::mutex Protection         |
        |   - ReservationTable (20 Seats)   |
        +-----------------+-----------------+
                          | (mq_send)
                          v
         [/dev/mqueue/css223_client_<ID>_reply]
                          |
                          v
                 Client Displays Result
```

- **องค์ประกอบหลัก:**
  - **Clients:** ทำงานแยกโปรเซส ส่งคำขอและรอรับคำตอบผ่านคิวส่วนตัว
  - **Server Queue:** `/css223_reservation_requests` รับคำขอจากไคลเอนต์ทั้งหมด
  - **Worker Pool:** เธรดคนงานดึงงานไปทำคู่ขนาน
  - **Client Reply Queue:** `/css223_client_<id>_reply` ส่งผลลัพธ์กลับไปยังไคลเอนต์ที่ถูกต้อง

### บทพูดผู้นำเสนอ (Speaker Notes):
> "นี่คือภาพรวมสถาปัตยกรรมของระบบครับ  
> ไคลเอนต์แต่ละตัวจะรันเป็นโปรเซสอิสระ เมื่อผู้ใช้กดจอง ไคลเอนต์จะส่ง Message Struct เข้าคิวกลางของเซิร์ฟเวอร์  
> ฝั่งเซิร์ฟเวอร์จะมี Worker Thread Pool คอยดึงคำขอจากคิวไปประมวลผล จากนั้นจะเข้าสู่กระบวนการล็อก Mutex เพื่ออ่านและเขียนตารางที่นั่ง 20 ที่นั่งใน RAM  
> เมื่อประมวลผลเสร็จแล้ว Worker จะส่งผลลัพธ์กลับไปยังคิวเฉพาะตัวของไคลเอนต์นั้นๆ ทำให้ไม่มีการส่งข้อมูลสับสนข้ามคนครับ"

---

## สไลด์ที่ 5: การออกแบบโปรโตคอลและโครงสร้างข้อความ IPC (Data Structures)

### เนื้อหาบนสไลด์:
- **ข้อกำหนดความปลอดภัยของ IPC (Memory Safety):**
  - ข้อมูลที่ส่งข้ามโปรเซสผ่าน Message Queue **ห้ามมี Pointer หรือ Dynamic Memory (`std::string`)** เพราะแอดเดรสของหน่วยความจำคนละโปรเซสไม่ตรงกัน
  - ใช้ **Raw Trivially Copyable Structs** ขนาดคงที่
- **RequestMessage (ขนาด 48 Bytes):**
  - `client_id` (uint32_t): หมายเลขระบุตัวตนของไคลเอนต์
  - `command` (enum): คำสั่ง เช่น `RESERVE`, `CANCEL`, `STATUS`, `LIST`, `QUIT`
  - `seat_id` (char[8]): รหัสที่นั่ง เช่น `"A1"`, `"B3"`
  - `reply_queue_name` (char[32]): ชื่อคิวตอบกลับ เช่น `"/css223_client_1_reply"`
- **ResponseMessage (ขนาด 48 Bytes):**
  - `result` (enum): รหัสสถานะ `Success` หรือ `Failure`
  - `status` (enum): สถานะที่นั่ง `Available` หรือ `Reserved`
  - `owner_client_id` (uint32_t): เจ้าของที่นั่งปัจจุบัน
  - `message` (char[32]): ข้อความแจ้งผล

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ในแง่การออกแบบโครงสร้างข้อมูล เราคำนึงถึงหลักการ Memory Isolation ของ OS เป็นหลัก  
> ข้อความทั้ง RequestMessage และ ResponseMessage จะต้องเป็น Trivially Copyable Struct และใช้ Fixed-size Character Array เท่านั้น เพื่อให้ Kernel สามารถคัดลอกข้อมูลดิบข้าม Address Space ของแต่ละโปรเซสได้อย่างปลอดภัยโดยไม่มีปัญหา Segmentation Fault ครับ"

---

## สไลด์ที่ 6: การตั้งค่าชุดการทดลอง 3 สถานการณ์ (Experimental Setup)

### เนื้อหาบนสไลด์:
- **ตัวแปรควบคุมในการทดลอง (Controlled Setup):**
  - ใช้ **5 ไคลเอนต์อิสระ** ส่งคำขอจอง **ที่นั่ง A1 เดียวกัน พร้อมกันในเสี้ยววินาที**
  - ตารางเปรียบเทียบพารามิเตอร์ 3 รูปแบบ:

| ตัวแปร / พารามิเตอร์ | การทดลองที่ 1<br>(Sequential Baseline) | การทดลองที่ 2<br>(Unsynchronized) | การทดลองที่ 3<br>(Synchronized) |
| :--- | :---: | :---: | :---: |
| **จำนวน Worker Threads** | `1 เธรด` | `5 เธรด` | `5 เธรด` |
| **สถานะ Mutex Lock** | เปิดใช้งาน | **ปิดใช้งาน (`--no-sync`)** | **เปิดใช้งาน (`std::mutex`)** |
| **การหน่วงเวลาสุ่ม (Random Delay)** | ปิด (0 ms) | **เปิด (50–500 ms)** | **เปิด (50–500 ms)** |
| **เป้าหมายที่นั่ง** | ที่นั่ง `A1` | ที่นั่ง `A1` | ที่นั่ง `A1` |
| **ผลลัพธ์ที่คาดการณ์** | FIFO ปลอดภัย ไม่มีแย่งชิง | **เกิด Race Condition / TOCTOU** | **Mutual Exclusion ป้องกันได้ 100%** |

### บทพูดผู้นำเสนอ (Speaker Notes):
> "เพื่อพิสูจน์พฤติกรรมของระบบปฏิบัติการ เราจึงออกแบบการทดลองออกเป็น 3 ชุด โดยส่งไคลเอนต์ 5 ตัวเข้าไปแย่งจองที่นั่ง A1 พร้อมกัน:  
> ชุดที่ 1 คือ Sequential ใช้ 1 เธรด เป็นตัวอ้างอิงพื้นฐาน  
> ชุดที่ 2 คือ Unsynchronized ใช้ 5 เธรด ปิด Mutex และใส่ Random Delay 50 ถึง 500 มิลลิวินาที เพื่อจำลองจังหวะที่ CPU Scheduler สลับการทำงาน  
> และชุดที่ 3 คือ Synchronized ใช้ 5 เธรด เปิดใช้ Mutex คลุมกระบวนการทั้งหมดครับ"

---

## สไลด์ที่ 7: ผลการทดลองที่ 1 - Sequential Baseline (1 Worker)

### เนื้อหาบนสไลด์:
- **บันทึก Server Log จริง:**
  ```text
  [Server] Workers: 1 | Synchronization: ENABLED | Random Delay: DISABLED
  [Worker 0] [CHECK] Seat A1 is AVAILABLE for Client 3
  [Worker 0] [UPDATE] Seat A1 SUCCESS: Reserved by Client 3
  [Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
  [Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 5
  [Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
  [Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 1
  ```
- **ผลลัพธ์ของไคลเอนต์:**
  - Client 3 (มาถึงคิวแรก): `[SUCCESS]` จองสำเร็จ
  - Client 1, 2, 4, 5: `[FAILED]` คำขอถูกปฏิเสธ
- **การวิเคราะห์ทาง OS:**
  - ทำงานตามลำดับคิว FIFO
  - ไม่มี Context Switch ข้ามเธรดใน Critical Section จึงไม่เกิดความขัดแย้งของข้อมูล

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ผลการทดลองที่ 1 เมื่อมี Worker เพียงตัวเดียว คำขอจะถูกนำมาประมวลผลทีละคำขอตามลำดับ FIFO ในคิว  
> เมื่อคำขอแรกของ Client 3 ได้รับการจองเรียบร้อย คำขอที่ 2 ถึง 5 ที่ตามมา เมื่อ Worker เข้าไปตรวจสอบ จะอ่านพบสถานะล่าสุดทันทีว่าที่นั่งถูกจองแล้ว จึงปฏิเสธคำขอตั้งแต่ขั้นตอน CHECK นี่คือสภาพแวดล้อมที่ไร้ปัญหา Concurrency ครับ"

---

## สไลด์ที่ 8: ผลการทดลองที่ 2 - Unsynchronized (Race Condition & TOCTOU)

### เนื้อหาบนสไลด์:
- **บันทึก Server Log จริง (จุดเกิดบั๊ก):**
  ```text
  [Worker 1] [CHECK] Seat A1 is AVAILABLE for Client 3   <-- จุดที่ 1: ทุกเธรดตรวจ RAM พร้อมกัน
  [Worker 2] [CHECK] Seat A1 is AVAILABLE for Client 1
  [Worker 3] [CHECK] Seat A1 is AVAILABLE for Client 2
  [Worker 0] [CHECK] Seat A1 is AVAILABLE for Client 5
  [Worker 4] [CHECK] Seat A1 is AVAILABLE for Client 4
  [Worker 2] [DELAY] Simulating delay: 160 ms
  [Worker 2] [UPDATE] Seat A1 SUCCESS: Reserved by Client 1  <-- จุดที่ 2: เธรด 2 จองสำเร็จก่อน
  [Worker 3] [DELAY] Simulating delay: 187 ms
  [Worker 3] [UPDATE] CONFLICT / DOUBLE BOOKING: FAILED!    <-- จุดที่ 3: เธรดอื่นพยายามเขียนทับ!
  ```
- **การวิเคราะห์ปรากฏการณ์ TOCTOU:**
  1. **Time-of-Check:** เธรดทั้ง 5 ตัวอ่าน RAM พร้อมกันและเห็นว่า A1 ว่าง
  2. **Interleaved Delay Window:** แต่ละเธรดเข้าสู่ช่วงหน่วงเวลาโดยไม่มีใครล็อกขวาง
  3. **Time-of-Use:** Worker 2 ดีเลย์น้อยสุด (160ms) เขียนสำเร็จ แต่เธรดที่เหลือตื่นมาเขียนทีหลัง เกิดสภาวะ **Double Booking Conflict**

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ผลการทดลองที่ 2 นี่คือจุดสำคัญที่สุดของโปรเจกต์ครับ  
> ดูที่ Log บรรทัดแรกๆ จะเห็นชัดเจนว่า เธรดทั้ง 5 ตัวเข้าไปทำขั้นตอน CHECK ในหน่วยความจำพร้อมกัน และต่างคนต่างอ่านได้ข้อมูลว่าที่นั่ง A1 ว่าง!  
> พอเข้าสู่ช่วงดีเลย์ Worker 2 สุ่มได้เวลาสั้นที่สุดคือ 160 ms จึงตื่นขึ้นมาก่อนและเขียนจองให้ Client 1 สำเร็จ  
> แต่พอ Worker 3, 1, 0, 4 ตื่นขึ้นมาตามลำดับ จะพยายามเขียนทับลงไปตามข้อมูลเก่าที่อ่านไว้ตอนแรก ทำให้ระบบตรวจพบว่าข้อมูลใน RAM ถูกแอบเปลี่ยนไปแล้ว จึงเกิดข้อผิดพลาด CONFLICT / DOUBLE BOOKING  
> นี่คือการจำลองช่องโหว่ TOCTOU ในระดับระบบปฏิบัติการอย่างเป็นรูปธรรมครับ"

---

## สไลด์ที่ 9: ผลการทดลองที่ 3 - Synchronized Concurrency (Mutual Exclusion via Mutex)

### เนื้อหาบนสไลด์:
- **บันทึก Server Log จริง:**
  ```text
  [Worker 1] [CHECK] Seat A1 is AVAILABLE for Client 3
  [Worker 1] [DELAY] Simulating delay: 385 ms for Seat A1   <-- Worker 1 ถือ Mutex Lock
  [Worker 1] [UPDATE] Seat A1 SUCCESS: Reserved by Client 3
  [Worker 0] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3) <-- Worker 0 ได้คิวถัดไป
  [Worker 0] [UPDATE] Seat A1 reservation FAILED for Client 4
  [Worker 2] [CHECK] Seat A1 is ALREADY RESERVED (Owner: Client 3)
  [Worker 2] [UPDATE] Seat A1 reservation FAILED for Client 2
  ```
- **การวิเคราะห์คุณสมบัติ Mutual Exclusion:**
  - กลไก `std::scoped_lock` ล็อกคุ้มกันลำดับ `CHECK -> DELAY -> UPDATE` เป็น **Atomic Sequence**
  - ในระหว่างที่ Worker 1 ดีเลย์ 385 ms เธรดตัวอื่นจะอยู่ในสถานะ **Blocked** ที่ระดับ Kernel
  - เมื่อ Worker 1 ปล่อย Lock เธรดถัดไปจึงเข้าไปอ่านสถานะจริงที่เป็นปัจจุบัน ขจัดปัญหา Stale Read สมบูรณ์แบบ 100%

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ผลการทดลองที่ 3 เมื่อเราเปิดใช้ `std::mutex` คลุมขั้นตอน CHECK, DELAY, และ UPDATE ทั้งหมด  
> จะสังเกตเห็นว่า ในขณะที่ Worker 1 กำลังดีเลย์ยาวนานถึง 385 ms ไม่มีเธรดอื่นใดสามารถแทรกตัวเข้ามาได้เลย เพราะเธรดตัวอื่นถูก Kernel สั่ง Blocked รออยู่หน้า Critical Section  
> เมื่อ Worker 1 จองสำเร็จและปล่อย Lock เธรดตัวถัดไปที่ได้รับ Lock เข้าไปตรวจ จะเห็นสถานะจริงทันทีว่าที่นั่งถูกจองไปแล้ว จึงปฏิเสธคำขออย่างถูกต้อง การันตีคุณสมบัติ Mutual Exclusion และความถูกต้องของข้อมูล 100% ครับ"

---

## สไลด์ที่ 10: ตารางเปรียบเทียบผลการทดลองทั้ง 3 การทดลอง (Summary Matrix)

### เนื้อหาบนสไลด์:

| หัวข้อเปรียบเทียบ | การทดลองที่ 1<br>(Sequential Baseline) | การทดลองที่ 2<br>(Unsynchronized) | การทดลองที่ 3<br>(Synchronized) |
| :--- | :---: | :---: | :---: |
| **จำนวน Worker Threads** | 1 เธรด | 5 เธรด | 5 เธรด |
| **ระดับ Concurrency** | Non-concurrent | **Concurrent** | **Concurrent** |
| **สถานะ Mutex Lock** | เปิด (แต่มีเธรดเดียว) | **ปิด (`--no-sync`)** | **เปิด (`std::mutex`)** |
| **การเกิด TOCTOU Stale Read** | ไม่เกิด | **เกิดพร้อมกัน 5 เธรด** | **ไม่เกิด (Atomic)** |
| **การตรวจพบ Double Booking** | 0 ครั้ง | **ตรวจพบ 4 เธรด** | **0 ครั้ง** |
| **จุดที่ปฏิเสธคำขอซ้ำซ้อน** | ขั้นตอน `CHECK` (ปกติ) | ขั้นตอน `UPDATE` (ขัดแย้งใน RAM) | ขั้นตอน `CHECK` (ถูกต้อง) |
| **การันตี Mutual Exclusion** | N/A | **ล้มเหลว (Violated)** | **สมบูรณ์แบบ (Guaranteed)** |

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ตารางนี้สรุปภาพรวมทั้งหมดอย่างชัดเจนครับ:  
> การทดลองที่ 1 ไม่มีปัญหา Concurrency เพราะมีเธรดเดียว  
> การทดลองที่ 2 เกิดการแย่งชิงและละเมิด Mutual Exclusion ชัดเจน โดยตรวจพบข้อผิดพลาดที่จังหวะ UPDATE  
> ส่วนการทดลองที่ 3 กลไก Mutex สามารถแก้ไขปัญหานี้ได้อย่างสมบูรณ์แบบ ทำให้คำขอซ้ำถูกตรวจพบและปฏิเสธอย่างถูกต้องตั้งแต่ขั้นตอน CHECK ครับ"

---

## สไลด์ที่ 11: วิธีการทดสอบและการันตีคุณภาพ (How to Test & Verification)

### เนื้อหาบนสไลด์:
- **คำสั่งรันชุดทดสอบอัตโนมัติ (Automated Test Execution):**
  ```bash
  # รันการทดสอบ Unit, Integration, Concurrency และ ThreadSanitizer
  ctest --preset test-unit
  ctest --preset test-integration
  ctest --preset test-concurrency
  ctest --preset test-tsan
  ```
- **ระดับการทดสอบที่ผ่าน 100%:**
  1. **Unit Tests (1/1):** ทดสอบกฎการจอง/ยกเลิก และที่นั่งมาตรฐาน 20 ที่นั่ง
  2. **Integration Tests (5/5):** ทดสอบการส่งรับผ่าน `/dev/mqueue` จริง, การดักจับสัญญาณ `EINTR`
  3. **Concurrency Tests (2/2):** ทดสอบการทำงานของ Worker Thread Pool
  4. **ThreadSanitizer (TSan):** คอมไพล์ด้วย `-fsanitize=thread` ยืนยัน **0 Data Races**
  5. **Resource Cleanup:** ดักจับสัญญาณ `SIGINT` (Ctrl+C) สั่ง `mq_unlink()` ลบคิวออกจาก Kernel ทุกครั้ง ไม่มีขยะค้างในระบบ

### บทพูดผู้นำเสนอ (Speaker Notes):
> "ในแง่ของกระบวนการทดสอบ (How to Test) ระบบของเรามีชุดการทดสอบครบทุกระดับผ่าน CTest:  
> มี Unit Tests ตรวจสอบตรรกะการจอง, Integration Tests ตรวจสอบการส่งรับผ่าน Message Queue จริงบน Linux, และ Concurrency Tests  
> นอกจากนี้เรายังใช้เครื่องมือระดับมาตรฐานอย่าง **ThreadSanitizer (TSan)** ยืนยันว่าไม่มี Data Race เกิดขึ้นในหน่วยความจำแม้แต่จุดเดียว  
> และเมื่อปิดโปรแกรม ระบบจะเรียก `mq_unlink` เสมอ ทำให้ไม่มี Resource ค้างในเคอร์เนลของ Linux ครับ"

---

## สไลด์ที่ 12: บทสรุปและประโยชน์ที่ได้รับ (Conclusion & Key Takeaways)

### เนื้อหาบนสไลด์:
- **สิ่งที่ได้เรียนรู้จากโครงงาน:**
  1. **IPC Primitives:** เข้าใจการทำงานของ POSIX Message Queue ข้อจำกัดของพอยน์เตอร์ และการจัดการคิวใน Linux VFS
  2. **Concurrency Pitfalls:** สัมผัสและพิสูจน์ปัญหา Race Condition และ TOCTOU ด้วย Log เชิงประจักษ์
  3. **Synchronization Correctness:** เข้าใจความสำคัญของการล็อกแบบ Compound Atomic Sequence (ไม่ใช่ล็อกแค่ตอนเขียน)
  4. **Production Cleanliness:** การจัดการ Signal Handling, Graceful Shutdown, และการคืนทรัพยากรให้ OS
- **เครื่องมือเสริมเพื่อการนำเสนอ:**
  - **Terminal Kiosk Preview:** จอภาพยนตร์ 3D โค้งสมจริงจัดกึ่งกลางหน้าจอ
  - **Interactive Web Visualizer:** แดชบอร์ดจำลองภาพการทำงานสด (`docs/visualizer/index.html`)

### บทพูดผู้นำเสนอ (Speaker Notes):
> "สรุปบทเรียนสำคัญจากโครงงานนี้คือ เราได้เห็นพฤติกรรมจริงของระบบปฏิบัติการ ทั้งการทำงานของ Message Queue ในเคอร์เนล และการแย่งชิงทรัพยากรของเธรด  
> เราได้ข้อสรุปว่าในการแก้ปัญหา Concurrency เราไม่สามารถล็อกแค่จังหวะเขียน (UPDATE) ได้ แต่ต้องล็อกคลุมตั้งแต่จังหวะอ่าน (CHECK) เพื่อป้องกันปัญหา TOCTOU  
> โครงงานนี้ผ่านการทดสอบและมีบันทึก Log จริงครบถ้วนตามเกณฑ์ของวิชา CSS223 ทุกประการครับ  
> ขอขอบคุณอาจารย์และเพื่อนๆ ทุกคนครับ ยินดีรับฟังคำถามและข้อเสนอแนะเพิ่มเติมครับ"
