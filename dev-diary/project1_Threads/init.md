# Project 1 Threads 구현일지

## Threads의 동작원리

## Threads에서 구현해야하는 함수
### Threads 폴더
- loader.S, loader.h
  - 수정X, 볼 필요X

- kernel.lds.S
  - 수정X, 궁금하면 봐도 됨

- init.c, init.h
  - 수정할 필요 없지만 원하면 수정해도 됨.
  - 커널 초기화, main()을 포함한다. main()을 최소한 확인해보고, 자신의 초기화 코드를 추가할 수 있다.

- **thread.c, thread.h**
  - implement O
  - 스레드 기본 베이스 파일, 대부분의 스레드 작업을 이 파일에서 할 예정.
  -  thread.h는 struct thread를 정의하고, 네 개의 프로젝트에서 다 사용한다. Threads에 대한 자세한 내용은 [Threads](https://casys-kaist.github.io/pintos-kaist/appendix/threads.html)

- **palloc.c, palloc.h**
  - implement O
  - Page allocator. 4kB 페이지 단위로 시스템 메모리를 할당. Page Allocator에 대한 자세한 내용 [Page Allocator](https://casys-kaist.github.io/pintos-kaist/appendix/memory_allocation.html#Page%20Allocator)

- malloc.c, malloc.h
  - 수정 X
  - malloc() 및 free() 간단한 구현. Block Allocator에 대한 자세한 내용은 ([Block Allocator](https://casys-kaist.github.io/pintos-kaist/appendix/memory_allocation.html#Block%20Allocator))

- interrupt.c, interrupt.h
  - 수정 X
  - interrupt 기본 처리 및 interrupt 켜고 끄기가 구현됨.

- **intr-stubs.S, intr-stubs.h**
  - implement O
  - low-level interrupt 처리를 위한 어셈블리 코드.

- synch.c, synch.h
  - 수정 X
  - Basic synchronization 코드: semaphores, locks, condition variables, optimization barriers. 네 개의 프로젝트에서 synchronization을 사용할 것이다. Synchronization에 대한 자세한 내용은 [Synchronization](https://casys-kaist.github.io/pintos-kaist/appendix/synchronization.html)

- mmu.c, mmu.h
  - 수정 X
  - x86-64 페이지 테이블 연산. lab1 이후 이 파일을 더 자세히 살펴볼 예정

- io.h
  - 수정 X
  - I/O 포트 액세스를 위한 함수들. 이 코드는 devices 디렉토리에 있는 소스 코드에서 사용되며, 수정할 필요는 없다.

- vaddr.h, pte.h
  - 수정 X
  - 가상 주소 및 페이지 테이블 항목을 작업하는 함수 및 매크로. 이 코드는 project 3에서 더 중요할 것이다.

- flags.h
  - 수정 X, 볼 필요 X
  - x86-64 flags 레지스터의 몇 비트를 정의하는 매크로. 

### devices 폴더
- **timer.c, timer.h**
  - implement O
  - System timer (ticks, 100 times per second).

- vga.c, vga.h
  - 수정 X, 볼 필요 X, printf() 함수가 이 코드를 호출한다.
  - VGA display driver. 화면에 글씨를 써준다.

- serial.c, serial.h
  - 수정 X, 볼 필요 X, printf() 함수가 이 코드를 호출한다.
  - Serial port driver.

- block.c, block.h
  - 수정 X
  - An abstraction layer for block devices, random-access, disk-like devices (고정된 크기의 블럭 배열로 구성된). 기본적으로 Pintos는 두 가지 유형의 블럭 디바이스를 지원한다:  IDE disks and partitions. project 2까지 사용안함.

- ide.c, ide.h
  - 수정 X
  - IDE 디스크에서 섹터 읽기와 쓰기를 지원한다.

- partition.c, partition.h
  - 수정 X
  - 디스크의 파티션 구조를 이해하고, 단일 디스크를 여러 영역(파티션)으로 나누어 독립적으로 사용할 수 있도록 한다.

- kbd.c, kbd.h
  - 수정 X
  - 키보드 드라이버. 키보드 입력을 처리하고, 입력 레이어로 전달한다.

- input.c, input.h
  - 수정 X
  - 입력 레이어. 키보드 또는 시리얼 드라이버에서 전달된 입력 문자를 큐에 넣는다.

- intq.c, intq.h
  - 수정 X
  - Interrupt queue, circular queue. 커널 스레드와 인터럽트 핸들러가 접근하고 싶어하는 큐. 키보드와 시리얼 드라이버에서 사용된다.

- rtc.c, rtc.h
  - 수정 X
  - 실시간 시계 드라이버. 커널이 현재 날짜와 시간을 결정할 수 있도록 한다. 기본적으로 이 코드는 thread/init.c에서 랜덤 넘버 생성기를 위한 초기 시드를 선택하는 데 사용된다.

- speaker.c, speaker.h
  - 수정 X
  - PC 스피커에서 소리를 내는 드라이버.

- pit.c, pit.h
  - 수정 X
  - 8254 Programmable Interrupt Timer 설정 코드. 이 코드는 devices/timer.c와 devices/speaker.c에서 사용된다. 각 디바이스는 PIT의 출력 채널 중 하나를 사용하기 때문이다.

### lib 폴더
- lib 폴더와 lib/kernel 폴더에는 유용한 라이브러리 루틴들이 포함되어 있습니다. (lib/user는 프로젝트 2부터 사용자 프로그램에서 사용될 예정이며, 커널의 일부는 아닙니다.)
- **기본 라이브러리 파일들**
  - 표준 C 라이브러리 하위 집합
    - ctype.h, inttypes.h, limits.h, stdarg.h, stdbool.h, stddef.h, stdint.h
    - stdio.c/h, stdlib.c/h, string.c/h
  - 디버깅 도구
    - debug.c/h: 디버깅을 돕는 함수들과 매크로 제공
  - 유틸리티 파일들
    - random.c/h: 의사 난수 생성기 (Pintos 실행마다 동일한 시퀀스 생성)
    - round.h: 반올림 관련 매크로
    - syscall-nr.h: 시스템 콜 번호 (프로젝트 2부터 사용)
- **kernel 하위 폴더 주요 파일들**
  - **자료구조 구현**
    - **list.c/h: 이중 연결 리스트 구현**
      - Pintos 전반에 걸쳐 사용됨
      - 프로젝트 1에서 직접 사용할 가능성이 높음
      - 시작 전 헤더 파일의 주석을 꼭 읽어볼 것을 권장
    - bitmap.c/h: 비트맵 구현
      - 필요한 경우 사용 가능하나 프로젝트 1에서는 거의 사용하지 않음
    - hash.c/h: 해시 테이블 구현
      - 프로젝트 3에서 유용하게 사용될 예정
  - **콘솔 출력 관련**
    - console.c/h, stdio.h
      - printf() 및 관련 함수들 구현
