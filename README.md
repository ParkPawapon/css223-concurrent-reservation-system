# Concurrent Cinema Seat Reservation System

คู่มือการติดตั้ง พัฒนา และทดสอบระบบสำรองที่นั่งโรงภาพยนตร์แบบทำงานร่วมกัน (CSS223 Operating Systems)

---

## 1. ภาพรวมโครงการ (Project Overview)

โครงการนี้เป็นส่วนหนึ่งของวิชา CSS223 Operating Systems โดยมีวัตถุประสงค์เพื่อศึกษาและทำความเข้าใจกลไกระดับระบบปฏิบัติการ (Operating System Primitives) ผ่านการสร้างระบบสำรองที่นั่งโรงภาพยนตร์แบบจำลองที่รองรับการทำงานพร้อมกัน (Concurrency):

- **Inter-Process Communication (IPC)**: สื่อสารระหว่างกระบวนการไคลเอนต์ (Client Processes) หลายตัวและเซิร์ฟเวอร์ (Server Process) ผ่าน POSIX Message Queues (`<mqueue.h>`, `-lrt`)
- **Multi-threading & Worker Pool**: เซิร์ฟเวอร์ทำงานแบบมัลติเธรด จัดการประมวลผลคำขอจองที่นั่งผ่านเธรดพูล (Worker Threads)
- **Mutual Exclusion & Synchronization**: ป้องกันสภาวะการแย่งชิงทรัพยากร (Race Conditions) และการจองที่นั่งซ้ำซ้อน (Double Booking) โดยใช้ `std::mutex` และ `std::lock_guard` ป้องกัน Critical Section ในตารางที่นั่งส่วนกลาง (`ReservationTable`)
- **Controlled Concurrency Experiments**: จำลองสภาวะแวดล้อมเพื่อเปรียบเทียบผลลัพธ์ระหว่างการทำงานแบบตามลำดับ (Sequential), แบบทำงานพร้อมกันโดยไม่มีการซิงโครไนซ์ (Concurrent Unsynchronized), และแบบทำงานพร้อมกันโดยมีการซิงโครไนซ์ที่ถูกต้อง (Concurrent Synchronized)

### รายละเอียดระบบเบื้องต้น
- จำนวนที่นั่งในระบบ: 20 ที่นั่ง ได้แก่ แถว A ถึง D แถวละ 5 ที่นั่ง (`A1`–`A5`, `B1`–`B5`, `C1`–`C5`, `D1`–`D5`)
- จำนวนไคลเอนต์ขั้นต่ำในการทดลอง: 5 ไคลเอนต์อิสระ
- คำสั่งที่รองรับในโปรโตคอล: `LIST`, `STATUS <seat_id>`, `RESERVE <seat_id>`, `CANCEL <seat_id>`, `QUIT`
- คิวส่งคำขอส่วนกลาง (Server Request Queue): `/css223_reservation_requests`
- คิวรับคำตอบเฉพาะไคลเอนต์ (Client Reply Queue): `/css223_client_<client_id>_reply`
- การหน่วงเวลาสุ่ม (Random Delay): 50 ถึง 500 มิลลิวินาที (ms) ระหว่างขั้นตอนการตรวจสอบสถานะและการบันทึกข้อมูลเพื่อเน้นผลกระทบของ Race Condition ในการทดลอง

---

## 2. ข้อจำกัดสำคัญด้านระบบปฏิบัติการ (Operating System Constraint)

โปรเจกต์นี้พึ่งพากลไกของ **Linux Kernel** เป็นหลัก โดยเฉพาะ POSIX Message Queues (`<mqueue.h>`, `-lrt`) และ POSIX Threading

- **ระบบปฏิบัติการ Windows**: คอมไพเลอร์ของ Windows ทั้ง MSVC และ MinGW **ไม่มี** ไลบรารี POSIX Message Queues (`<mqueue.h>`) ในตัว จึงไม่สามารถคอมไพล์หรือรันระบบแบบเนทีฟได้ ผู้ใช้ Windows **จำเป็นต้องพัฒนาผ่าน Docker Desktop หรือ WSL2 (Ubuntu 24.04)** เท่านั้น
- **ระบบปฏิบัติการ macOS**: แม้ว่า macOS จะเป็นระบบตระกูล POSIX แต่ Darwin Kernel ไม่รองรับมาตรฐาน POSIX Message Queues ตามรูปแบบที่ Linux ใช้งาน (ฟังก์ชัน `mq_open` บน macOS มีข้อจำกัดและถูกประกาศเป็น deprecated) ดังนั้นผู้ใช้ macOS ทั้งสถาปัตยกรรม Intel และ Apple Silicon (M1/M2/M3/M4) **ต้องใช้งานผ่าน Docker Desktop**
- **มาตรฐานการพัฒนาของทีม**: เพื่อให้สภาพแวดล้อมในการพัฒนาเหมือนกันทุกประการ และตรงกับระบบตรวจสอบอัตโนมัติบน GitHub Actions CI ขอให้สมาชิกทุกคนพัฒนาและรันชุดทดสอบผ่าน **Docker Development Container** หรือ **Ubuntu 24.04 บน WSL2** เป็นหลัก

---

## 3. สิ่งที่ต้องติดตั้งล่วงหน้า (Prerequisites)

ดาวน์โหลดและติดตั้งเครื่องมือที่จำเป็นสำหรับระบบปฏิบัติการของท่านจากแหล่งข้อมูลอย่างเป็นทางการ:

| เครื่องมือ | เวอร์ชันขั้นต่ำที่แนะนำ | วัตถุประสงค์ | ลิงก์ดาวน์โหลดอย่างเป็นทางการ |
|---|---|---|---|
| Git | 2.40+ | ระบบควบคุมเวอร์ชันโค้ด | [https://git-scm.com/](https://git-scm.com/) |
| Docker Desktop | ล่าสุด | รัน Development Container และเครื่องมือจำลอง Linux | [https://www.docker.com/products/docker-desktop/](https://www.docker.com/products/docker-desktop/) |
| WSL2 (สำหรับ Windows) | Ubuntu 24.04 LTS | ระบบ Linux เสมือนบน Windows | [https://learn.microsoft.com/en-us/windows/wsl/install](https://learn.microsoft.com/en-us/windows/wsl/install) |
| Ubuntu Linux | 24.04 LTS | สภาพแวดล้อมระบบปฏิบัติการเป้าหมาย | [https://ubuntu.com/](https://ubuntu.com/) |
| CMake | 3.28+ | ระบบกำหนดค่าการคอมไพล์ (Build System Generator) | [https://cmake.org/download/](https://cmake.org/download/) |
| Ninja | 1.11+ | ระบบคอมไพล์ความเร็วสูง (Build Engine) | [https://ninja-build.org/](https://ninja-build.org/) |
| Homebrew (สำหรับ macOS) | ล่าสุด | ระบบจัดการแพ็กเกจบน macOS | [https://brew.sh/](https://brew.sh/) |

---

## 4. โครงสร้างซอร์สโค้ดและหน้าที่ของแต่ละโมดูล (Repository Architecture)

โครงการจัดโครงสร้างโค้ดแบบแยกหน้าที่ (Separation of Concerns) อย่างเคร่งครัดตามไดเรกทอรีดังนี้:

```text
.
├── include/
│   ├── client/           # ส่วนหัวสำหรับ Client และ Command Parser
│   ├── common/           # โครงสร้างข้อมูลร่วม ค่าคงที่ และรหัสผลลัพธ์
│   ├── concurrency/      # เครื่องมือสร้างการหน่วงเวลาแบบสุ่ม
│   ├── core/             # โดเมนโมเดล ที่นั่ง (Seat) และตารางสำรอง (ReservationTable)
│   ├── ipc/              # โครงสร้างข้อความ คิว POSIX Message Queue และชื่อคิว
│   └── server/           # ส่วนหัวสำหรับ Server, Worker Pool, และ Request Processor
├── src/
│   ├── client/           # การทำงานของ Client และฟังก์ชันหลัก client_main
│   ├── concurrency/      # การหน่วงเวลาสุ่ม (Random Delay Generator)
│   ├── core/             # ลอจิกการจัดการที่นั่งและ Mutex Critical Section
│   ├── ipc/              # การส่งรับข้อความดิบผ่าน POSIX Message Queue
│   └── server/           # การประมวลผลคำขอ เธรดพูล และฟังก์ชันหลัก server_main
├── tests/
│   ├── concurrency/      # การทดสอบ Concurrency และ Random Delay
│   ├── integration/      # การทดสอบโครงสร้างข้อความและการตั้งชื่อคิว IPC
│   └── unit/             # การทดสอบ Unit Test สำหรับโดเมนโมเดล
├── docs/
│   └── experiments/      # บันทึกผลการทดลอง (Logs และ Screenshots) สำหรับ Exp 1, 2, 3
├── cmake/                # โมดูล CMake สำหรับ Compiler Flags, Warnings, Sanitizers
├── CMakeLists.txt        # ไฟล์กำหนดค่าโปรเจกต์ระดับราก (ห้ามคอมไพล์แบบ In-source)
├── CMakePresets.json     # กำหนดค่า Preset การ Build และ Test แบบมาตรฐาน
├── compose.yaml          # การตั้งค่า Docker Compose สำหรับ Development และ Sanitizers
└── Dockerfile            # Multi-stage Dockerfile บน Ubuntu 24.04
```

### รายละเอียดไลบรารีและไบนารีที่ถูกสร้าง
- `reservation_common` (INTERFACE): เฮดเดอร์ส่วนกลางสำหรับ Type Definitions, Constants, Status Codes
- `reservation_core` (STATIC): โดเมนโมเดลที่นั่งและการจัดการตารางในหน่วยความจำ
- `reservation_concurrency` (STATIC): ระบบจำลอง Random Delay สำหรับการทดลอง Race Condition
- `reservation_ipc` (STATIC): ตัวครอบ POSIX Message Queue และการจัดการโครงสร้างข้อความแบบ Trivially Copyable
- `reservation_server_lib` (STATIC): คลาส Server, เธรดพูล WorkerPool, และตัวประมวลผล RequestProcessor
- `reservation_client_lib` (STATIC): คลาส Client และตัวแยกคำสั่ง CommandParser
- `reservation_server` (EXECUTABLE): ไบนารีหลักของโปรเซสเซิร์ฟเวอร์
- `reservation_client` (EXECUTABLE): ไบนารีหลักของโปรเซสไคลเอนต์

---

## 5. ขั้นตอนเริ่มต้นใช้งานสำหรับสมาชิกใหม่ (Step-by-Step Getting Started)

ทำตามขั้นตอนด้านล่างนี้เพื่อโคลนโปรเจกต์ เปิดคอนเทนเนอร์ คอมไพล์ และตรวจสอบความพร้อมของระบบ:

### ขั้นตอนที่ 1: โคลน Repository ไปยังเครื่องของท่าน

เลือกคำสั่งตาม Shell ของระบบปฏิบัติการที่ท่านใช้งาน:

#### สำหรับ macOS และ Linux (Bash / Zsh):
```bash
git clone https://github.com/ParkPawapon/css223-concurrent-reservation-system.git
cd css223-concurrent-reservation-system
```

#### สำหรับ Windows (PowerShell):
```powershell
git clone https://github.com/ParkPawapon/css223-concurrent-reservation-system.git
Set-Location css223-concurrent-reservation-system
```

#### สำหรับ Windows (Command Prompt - CMD):
```cmd
git clone https://github.com/ParkPawapon/css223-concurrent-reservation-system.git
cd css223-concurrent-reservation-system
```

---

### ขั้นตอนที่ 2: เริ่มต้น Development Container ผ่าน Docker Compose

ตรวจสอบให้แน่ใจว่า Docker Desktop หรือ Docker Daemon กำลังทำงานอยู่ จากนั้นรันคำสั่งบนเครื่อง Host:

#### สำหรับ macOS / Linux:
```bash
docker compose up -d development
```

#### สำหรับ Windows (PowerShell):
```powershell
docker compose up -d development
```

#### สำหรับ Windows (CMD):
```cmd
docker compose up -d development
```

*หมายเหตุ: คำสั่งนี้จะสร้างอิมเมจจาก `Dockerfile` บนฐาน Ubuntu 24.04 ที่ติดตั้งคอมไพเลอร์ Clang, GCC, CMake, Ninja, และเครื่องมือตรวจสอบโค้ดไว้อย่างครบถ้วน และทำการเมานต์โฟลเดอร์โปรเจกต์เข้าสู่ไดเรกทอรี `/workspace` ภายในคอนเทนเนอร์แบบทันที (Bind Mount)*

---

### ขั้นตอนที่ 3: เข้าสู่ Bash Shell ภายใน Container

รันคำสั่งบนเครื่อง Host เพื่อเปิดหน้าต่างเทอร์มินัลเข้าไปทำงานใน Container:

#### ทุกแพลตฟอร์ม (macOS, Linux, Windows PowerShell, Windows CMD):
```bash
docker compose exec development bash
```

เมื่อเข้าสู่ Container แล้ว พร้อมท์จะเปลี่ยนเป็นลักษณะ:
```text
ubuntu@<container_id>:/workspace$
```

*คำสั่งทั้งหมดนับจากนี้ (ในหัวข้อที่ 6, 7, 8) จะรัน **ภายใน Container Shell** นี้*

---

### ขั้นตอนที่ 4: คอมไพล์ระบบด้วย CMake Presets (ภายใน Container)

โครงการนี้ใช้ **CMake Presets** ร่วมกับ **Ninja Build System** (ระบบได้บล็อกการคอมไพล์แบบ In-source build คือการรัน `cmake .` ไว้อย่างชัดเจนเพื่อป้องกันไฟล์ขยะปนเปื้อนใน Source Tree):

```bash
# คอมไพล์โหมด Debug ด้วย Workflow Preset (รันการ configure และ build ในขั้นตอนเดียว)
cmake --workflow --preset workflow-debug
```

หรือหากต้องการรันแยกขั้นตอนด้วยตนเอง:
```bash
# กำหนดค่าโปรเจกต์ (Configure)
cmake --preset debug

# ทำการคอมไพล์ (Build)
cmake --build --preset debug
```

ไฟล์ไบนารีที่ได้จะอยู่ที่:
- `build/debug/src/reservation_server`
- `build/debug/src/reservation_client`

---

### ขั้นตอนที่ 5: ตรวจสอบความถูกต้องด้วยชุดทดสอบ Smoke Tests (ภายใน Container)

รันชุดทดสอบเพื่อยืนยันว่าคอมไพเลอร์และสภาพแวดล้อมทำงานได้อย่างถูกต้อง:

```bash
# ทดสอบ Unit Tests สำหรับโดเมนโมเดล
cmake --workflow --preset workflow-test-unit

# ทดสอบ Integration Tests สำหรับ IPC Message และ Queue Names
cmake --workflow --preset workflow-test-integration

# ทดสอบ Concurrency Tests สำหรับ Random Delay Generator
cmake --workflow --preset workflow-test-concurrency
```

หากผลการทดสอบแสดงสถานะ `100% tests passed` แสดงว่าสภาพแวดล้อมพร้อมสำหรับการพัฒนาต่อยอด

---

## 6. คู่มือการรันโปรแกรมและการทดลอง (Execution & Experiments Guide)

### 6.1 การสั่งงานเซิร์ฟเวอร์ (`reservation_server`)

พารามิเตอร์ CLI ที่รองรับ:
- `--workers <N>`: กำหนดจำนวน Worker Threads (ค่าเริ่มต้นคือ 3)
- `--no-sync`: ปิดการทำงานของ Mutex Synchronization (ใช้สำหรับการทดลองที่ 2)
- `--delay`: เปิดการหน่วงเวลาแบบสุ่ม (50–500 มิลลิวินาที) ระหว่างขั้นตอนการตรวจสอบและอัปเดตที่นั่ง
- `--help`: แสดงข้อความช่วยเหลือการใช้งาน

ตัวอย่างการรันเซิร์ฟเวอร์:
```bash
# รันเซิร์ฟเวอร์แบบค่าเริ่มต้น (3 เธรด พร้อม Synchronization)
./build/debug/src/reservation_server

# รันเซิร์ฟเวอร์พร้อมหน่วงเวลาสุ่ม
./build/debug/src/reservation_server --workers 3 --delay
```

---

### 6.2 การสั่งงานไคลเอนต์ (`reservation_client`)

ไคลเอนต์รับพารามิเตอร์บังคับ 1 ค่า คือ `<client_id>` ซึ่งต้องเป็นจำนวนเต็มบวก:

```bash
# รันไคลเอนต์หมายเลข 1
./build/debug/src/reservation_client 1

# รันไคลเอนต์หมายเลข 2
./build/debug/src/reservation_client 2
```

---

### 6.3 คำสั่งในระบบสำรองที่นั่ง

เมื่อไคลเอนต์เชื่อมต่อกับเซิร์ฟเวอร์ ไคลเอนต์จะสามารถส่งคำขอได้ 5 รูปแบบ:
1. `LIST`: แสดงสถานะการจองของที่นั่งทั้งหมด 20 ที่นั่ง (`A1`–`D5`) พร้อมรหัสไคลเอนต์ที่เป็นเจ้าของ
2. `STATUS <seat_id>`: ตรวจสอบสถานะของที่นั่งเฉพาะเจาะจง เช่น `STATUS A1`
3. `RESERVE <seat_id>`: ขอจองที่นั่งที่ระบุ เช่น `RESERVE A1`
4. `CANCEL <seat_id>`: ขอยกเลิกการจองที่นั่งที่ตนเองเคยจองไว้ เช่น `CANCEL A1` (ไม่สามารถยกเลิกที่นั่งของผู้อื่นได้)
5. `QUIT`: แจ้งออกจากระบบและปิดการทำงานของไคลเอนต์

---

### 6.4 แนวทางการรันการทดลอง 3 รูปแบบตามโจทย์วิชา CSS223

การทดลองมีวัตถุประสงค์เพื่อศึกษาพฤติกรรมของระบบปฏิบัติการภายใต้เงื่อนไข Concurrency ที่แตกต่างกัน:

#### การทดลองที่ 1: การทำงานตามลำดับ (Experiment 1 - Sequential Baseline)
- **สมมติฐาน**: เมื่อเซิร์ฟเวอร์มีเพียง 1 Worker Thread คำขอทุกคำขอจะถูกประมวลผลเรียงตามลำดับอย่างสมบูรณ์ จะไม่มีปัญหา Race Condition หรือ Double Booking เกิดขึ้น
- **คำสั่งรันเซิร์ฟเวอร์**:
  ```bash
  ./build/debug/src/reservation_server --workers 1
  ```
- **การทดสอบ**: ส่งคำขอจากไคลเอนต์ 5 ตัวพร้อมกัน และบันทึกผลการทำงานลงในไดเรกทอรี `docs/experiments/experiment-01-sequential/`

#### การทดลองที่ 2: การทำงานพร้อมกันโดยไม่มีการซิงโครไนซ์ (Experiment 2 - Concurrent Unsynchronized)
- **สมมติฐาน**: เมื่อเปิดใช้ Worker Threads หลายตัว (>= 3) ร่วมกับการปิด Mutex (`--no-sync`) และเปิด Random Delay (`--delay`) จะเกิดสภาวะ Race Condition ทำให้เกิดการจองที่นั่งซ้ำ (Double Booking) หรือข้อมูลสถานะที่นั่งคลาดเคลื่อน
- **คำสั่งรันเซิร์ฟเวอร์**:
  ```bash
  ./build/debug/src/reservation_server --workers 3 --no-sync --delay
  ```
- **การทดสอบ**: ให้ไคลเอนต์หลายตัวส่งคำขอจองที่นั่งหมายเลขเดียวกัน (เช่น `RESERVE A1`) ในเวลาใกล้เคียงกัน บันทึกข้อผิดพลาดที่เกิดขึ้นลงในไดเรกทอรี `docs/experiments/experiment-02-without-synchronization/`

#### การทดลองที่ 3: การทำงานพร้อมกันโดยมีการซิงโครไนซ์ที่ถูกต้อง (Experiment 3 - Concurrent Synchronized)
- **สมมติฐาน**: เมื่อเปิดใช้ Worker Threads หลายตัว (>= 3) ร่วมกับ Random Delay (`--delay`) แต่เปิดการทำงานของ Mutual Exclusion (`std::mutex`) จะป้องกัน Critical Section ได้อย่างสมบูรณ์ โดยจะมีเพียง 1 ไคลเอนต์ที่จองสำเร็จ ส่วนคำขอที่เหลือจะได้รับแจ้งว่าที่นั่งถูกจองแล้ว
- **คำสั่งรันเซิร์ฟเวอร์**:
  ```bash
  ./build/debug/src/reservation_server --workers 3 --delay
  ```
- **การทดสอบ**: ทดสอบด้วยสถานการณ์เดียวกับการทดลองที่ 2 เพื่อยืนยันว่าระบบไม่มี Double Booking และข้อมูลสอดคล้องกัน 100% บันทึกผลลงในไดเรกทอรี `docs/experiments/experiment-03-with-synchronization/`

---

## 7. เครื่องมือตรวจสอบหน่วยความจำและดาต้าเรซ (Sanitizers & Quality Tools)

โครงการได้ตั้งค่าเครื่องมือ Sanitizers ของ LLVM/Clang เพื่อตรวจจับข้อผิดพลาดระดับลึกที่คอมไพเลอร์ทั่วไปตรวจไม่พบ:

### 7.1 AddressSanitizer (ASan) & LeakSanitizer (LSan)
ตรวจจับการเข้าถึงหน่วยความจำนอกขอบเขต (Out-of-bounds access), การใช้หน่วยความจำหลังถูกคืน (Use-after-free), และการรั่วไหลของหน่วยความจำ (Memory Leaks):
```bash
cmake --workflow --preset workflow-test-asan
```

### 7.2 UndefinedBehaviorSanitizer (UBSan)
ตรวจจับพฤติกรรมที่ไม่พึงประสงค์ตามมาตรฐาน C++ เช่น Integer Overflow, การเลื่อนบิตที่ผิดพลาด, หรือการ Dereference ค่า Null Pointer:
```bash
cmake --workflow --preset workflow-test-ubsan
```

### 7.3 ThreadSanitizer (TSan) สำหรับตรวจจับ Data Races
ตรวจจับการเข้าถึงตัวแปรเดียวกันจากหลายเธรดโดยไม่มี Synchronization (Data Race)

#### รันภายใน Container ปกติ:
```bash
cmake --workflow --preset workflow-test-tsan
```

#### หรือรันผ่าน Docker Service พิเศษจากเครื่อง Host:
เนื่องจาก ThreadSanitizer ใน LLVM 18 ต้องการพื้นที่ Shadow Memory ที่มีสิทธิ์เฉพาะตัว บนเครื่อง Host สามารถรันเซอร์วิส `test-tsan` ที่กำหนดค่า `seccomp=unconfined` ไว้ล่วงหน้าได้โดยตรง:
```bash
docker compose --profile tests run --rm test-tsan
```

---

## 8. มาตรฐานคุณภาพโค้ดและการจัดรูปแบบ (Code Quality & Formatting)

ก่อนเปิด Pull Request ทุกครั้ง สมาชิกในทีมต้องตรวจสอบให้แน่ใจว่าโค้ดผ่านการฟอร์แมตและการตรวจสอบแบบ Static Analysis:

### 8.1 การจัดรูปแบบโค้ดด้วย Clang-Format
โครงการใช้สไตล์ Google C++ Style ที่กำหนดไว้ในไฟล์ `.clang-format`:

```bash
# ตรวจสอบรูปแบบโค้ด (หากมีไฟล์ไม่ตรงตามรูปแบบจะฟ้องเตือน)
clang-format --style=file --dry-run --Werror \
    include/*/*.hpp src/*/*.cpp tests/*/*.cpp

# จัดรูปแบบโค้ดอัตโนมัติทุกไฟล์
clang-format -i --style=file \
    include/*/*.hpp src/*/*.cpp tests/*/*.cpp
```

### 8.2 การวิเคราะห์โค้ดเชิงสถิตด้วย Clang-Tidy
ตรวจสอบมาตรฐานโค้ด ข้อควรระวังด้านความปลอดภัย และประสิทธิภาพ ตามกฎใน `.clang-tidy`:

```bash
cmake --workflow --preset workflow-quality
```

*ข้อควรจำ: GitHub Actions CI จะตรวจสอบทั้ง Clang-Format และ Clang-Tidy อัตโนมัติทุกครั้งที่มีการเปิด PR หากพบข้อผิดพลาด CI จะไม่ผ่าน*

---

## 9. แนวทางการแบ่งงานและพัฒนาต่อยอด (Where to Put Your Code)

เพื่อป้องกันการแก้ไขโค้ดทับซ้อนกัน ขอให้สมาชิกดูแนวทางการนำโค้ดไปใส่ตามตารางนี้:

| งานที่ต้องพัฒนา | เฮดเดอร์ไฟล์ (`include/`) | ซอร์สไฟล์ (`src/`) | ชุดทดสอบที่เกี่ยวข้อง (`tests/`) |
|---|---|---|---|
| ปรับเปลี่ยนคำสั่งหรือโปรโตคอล IPC | `include/common/command.hpp`<br>`include/ipc/message.hpp` | `src/ipc/message.cpp` | `tests/integration/ipc_message_smoke_test.cpp` |
| แก้ไขลอจิกการจองที่นั่งและการใช้ Mutex | `include/core/reservation_table.hpp`<br>`include/core/seat.hpp` | `src/core/reservation_table.cpp`<br>`src/core/seat.cpp` | `tests/unit/core_model_smoke_test.cpp` |
| ปรับปรุงการหน่วงเวลาแบบสุ่ม (Delay) | `include/concurrency/random_delay.hpp` | `src/concurrency/random_delay.cpp` | `tests/concurrency/concurrency_skeleton_test.cpp` |
| ปรับปรุงการจัดการคิว POSIX Message Queue | `include/ipc/posix_message_queue.hpp` | `src/ipc/posix_message_queue.cpp` | `tests/integration/` |
| พัฒนาการประมวลผลคำขอและเธรดพูลฝั่งเซิร์ฟเวอร์ | `include/server/worker_pool.hpp`<br>`include/server/request_processor.hpp` | `src/server/worker_pool.cpp`<br>`src/server/request_processor.cpp` | `tests/integration/` |
| พัฒนาส่วนติดต่อผู้ใช้ (CLI/REPL) ฝั่งไคลเอนต์ | `include/client/command_parser.hpp`<br>`include/client/client.hpp` | `src/client/command_parser.cpp`<br>`src/client/client.cpp` | `tests/unit/` |
| จัดทำสคริปต์ทดสอบและบันทึกผลการทดลอง | - | - | บันทึกผลใน `docs/experiments/` |

---

## 10. วงจรการทำงานร่วมกันผ่าน Git (Developer Workflow)

1. **สร้าง Branch ใหม่จาก `main` ล่าสุดเสมอ**:
   ```bash
   git checkout main
   git pull origin main
   git checkout -b feat/<task-name>
   ```
   *ตัวอย่าง: `feat/client-interactive-repl`, `feat/table-mutex-locking`*

2. **เขียนโค้ดและทดสอบใน Container**:
   แก้ไขโค้ดจากโปรแกรมแก้ไขของท่านบนเครื่อง Host (เช่น VS Code หรือ CLion) จากนั้นสลับไปรันการคอมไพล์และทดสอบใน Container:
   ```bash
   cmake --workflow --preset workflow-debug
   cmake --workflow --preset workflow-test-unit
   ```

3. **จัดรูปแบบโค้ดและรัน Static Analysis**:
   ```bash
   clang-format -i --style=file include/*/*.hpp src/*/*.cpp tests/*/*.cpp
   cmake --workflow --preset workflow-quality
   ```

4. **ตรวจสอบสถานะและเปิด Pull Request**:
   ```bash
   git status
   git diff
   git add <files>
   git commit -m "feat(<module>): describe your change concisely"
   git push origin feat/<task-name>
   ```
   จากนั้นเข้าไปที่ GitHub เพื่อเปิด Pull Request เข้าสู่ Branch `main` และรอการตรวจสอบจาก CI

---

## 11. ตารางสรุปคำสั่งที่ใช้บ่อย (Host vs. Container Cheat Sheet)

| กิจกรรม | รันบนเครื่อง Host หรือ Container | คำสั่งที่ใช้ |
|---|---|---|
| สตาร์ต Development Container | **Host** | `docker compose up -d development` |
| เข้าสู่ Shell ใน Container | **Host** | `docker compose exec development bash` |
| หยุดการทำงานของ Container | **Host** | `docker compose down` |
| คอมไพล์โปรเจกต์ (Debug) | **Container** | `cmake --workflow --preset workflow-debug` |
| คอมไพล์โปรเจกต์ (Release) | **Container** | `cmake --workflow --preset workflow-release` |
| รัน Unit Tests | **Container** | `cmake --workflow --preset workflow-test-unit` |
| รัน Integration Tests | **Container** | `cmake --workflow --preset workflow-test-integration` |
| รัน Concurrency Tests | **Container** | `cmake --workflow --preset workflow-test-concurrency` |
| รันการทดสอบ ThreadSanitizer | **Container** | `cmake --workflow --preset workflow-test-tsan` |
| รันการทดสอบ ThreadSanitizer (Docker) | **Host** | `docker compose --profile tests run --rm test-tsan` |
| จัดรูปแบบโค้ดอัตโนมัติ | **Container** | `clang-format -i --style=file include/*/*.hpp src/*/*.cpp tests/*/*.cpp` |
| ตรวจสอบโค้ดด้วย Clang-Tidy | **Container** | `cmake --workflow --preset workflow-quality` |
| สตาร์ตเซิร์ฟเวอร์ | **Container** | `./build/debug/src/reservation_server [options]` |
| สตาร์ตไคลเอนต์ | **Container** | `./build/debug/src/reservation_client <client_id>` |

---

## 12. การติดตั้งแบบ Native บน Ubuntu 24.04 (ทางเลือกเสริม)

หากท่านใช้ Ubuntu 24.04 LTS แบบติดตั้งตรงบนเครื่องหรือใช้งานผ่าน WSL2 โดยไม่ต้องการใช้ Docker สามารถติดตั้งเครื่องมือทั้งหมดได้ผ่านคำสั่ง:

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    ca-certificates \
    clang \
    clang-format \
    clang-tidy \
    cmake \
    libclang-rt-dev \
    ninja-build \
    pkg-config
```

เมื่อติดตั้งเสร็จ สามารถใช้คำสั่ง CMake Presets ต่างๆ ได้เหมือนใน Container ทุกประการ

---

## 13. การแก้ไขปัญหาที่พบบ่อย (Troubleshooting)

### ปัญหาที่ 1: `In-source builds are not allowed`
- **สาเหตุ**: มีการเผลอรันคำสั่ง `cmake .` หรือ `cmake CMakeLists.txt` ที่โฟลเดอร์รากของโปรเจกต์
- **วิธีแก้**: ระบบบล็อกเพื่อไม่ให้สร้างไฟล์คอมไพล์ปนเปื้อนในโฟลเดอร์โค้ด ให้ลบไฟล์แคชทิ้งแล้วใช้งานผ่าน Preset:
  ```bash
  rm -rf CMakeCache.txt CMakeFiles/
  cmake --preset debug
  ```

### ปัญหาที่ 2: คิว POSIX Message Queue ค้างอยู่ในระบบ (`File exists` หรือเปิดคิวไม่ได้)
- **สาเหตุ**: โปรแกรมเซิร์ฟเวอร์หรือไคลเอนต์ถูกยกเลิกกะทันหันก่อนที่จะทำคำสั่ง `mq_unlink` ทำให้เคอร์เนลของ Linux ยังคงเก็บคิวไว้ใน Memory VFS
- **วิธีแก้**: ตรวจสอบและลบไฟล์คิวที่ขึ้นต้นด้วย `/css223_` ออกจาก `/dev/mqueue`:
  ```bash
  # ตรวจสอบรายการคิวที่ค้างอยู่
  ls -la /dev/mqueue

  # ลบคิวของโปรเจกต์
  rm -f /dev/mqueue/css223_*
  ```

### ปัญหาที่ 3: Docker บน Linux แจ้งเตือนเรื่องสิทธิ์ (Permission Denied)
- **สาเหตุ**: ผู้ใช้งานบน Linux ยังไม่ได้อยู่ในกลุ่ม `docker`
- **วิธีแก้**:
  ```bash
  sudo usermod -aG docker $USER
  newgrp docker
  ```

### ปัญหาที่ 4: ปัญหารหัสขึ้นบรรทัดใหม่ CRLF บน Windows
- **สาเหตุ**: Git บน Windows อาจแปลงไฟล์เป็น CRLF โดยอัตโนมัติ ทำให้สคริปต์เชลล์ใน Linux ทำงานผิดพลาด
- **วิธีแก้**: โปรเจกต์ได้กำหนดไฟล์ `.gitattributes` บังคับให้เป็น LF ไว้แล้ว แนะนำให้ตั้งค่า Git เพิ่มเติม:
  ```bash
  git config --global core.autocrlf false
  ```

---

## 14. สถานะปัจจุบันของโครงการ (Project Status & Roadmap)

โครงสร้างสถาปัตยกรรมหลักของระบบได้รับการติดตั้งและผ่านการตรวจสอบอย่างสมบูรณ์แล้ว:
- ระบบ CMake Presets, Ninja Generator, และการตั้งค่า Compiler Flags / Warnings / Sanitizers ทำงานสมบูรณ์
- โครงสร้างคลาส โดเมนโมเดล ข้อความ IPC และสเกเลตันของเธรดพูลพร้อมใช้งาน
- สภาพแวดล้อม Dockerfile, Docker Compose, และ GitHub Actions CI (12 matrix jobs) ผ่านการทดสอบ 100%
- Smoke tests ทั้ง 3 ระดับ (`unit`, `integration`, `concurrency`) ผ่านการทดสอบเรียบร้อย

**งานที่ทีมต้องร่วมกันพัฒนาใน Sprint ถัดไป**:
1. พัฒนาลูปการรับส่งคำสั่งแบบโต้ตอบ (Interactive REPL Loop) ใน `src/client/client.cpp`
2. พัฒนากลไกการดึงคำขอจากคิวและกระจายงานให้ Worker Thread ใน `src/server/request_processor.cpp`
3. เชื่อมต่อการล็อก `std::mutex` ใน `src/core/reservation_table.cpp` ให้สมบูรณ์เพื่อป้องกันการจองซ้ำ
4. ดำเนินการรันการทดลองทั้ง 3 รูปแบบ บันทึก Logs และจับภาพหน้าจอลงในโฟลเดอร์ `docs/experiments/`
