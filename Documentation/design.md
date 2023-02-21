# Switchtec 커널 설명서
해당 문서는 Switchtec 드라이버 전용 커널 코드 작업에 대한 기초적인 지식을 제공하는것을 목표로 하며,

코드를 수정하는데에 도움이 되는 몇가지 핵심 개념과 특이점을 설명합니다.

해당 문서는 최신화되지 않았을 수 있으므로, 코드와 함께 참고하는것이 좋습니다.



Switchtec 커널 모듈은 크게 2가지 파일, `switchtec.ko`와 `ntb_hw_switchtec`으로 나뉩니다.

`switchtec.ko`는 NTB 엔드포인트를 열거하고 구성하는것에 초점을 맞추고 있으며,

  Switchtec 커널 드라이버 개발자를 위한 인터페이스를 제공합니다.

`ntb_hw_switchtec.ko`는 Linux NTB 스택용 드라이버를 제공합니다.

  해당 파일은 `switchtec.ko`에 의존성을 갖습니다. 

## switchtec.ko
기본 Switchtec 드라이버는 Linux의 표준 방식으로 디바이스 목록을 열거합니다.
Linux의 기본 장치 열거 방식은 [LDD3][1]을 참고하십시오.


### 사용자 공간 인터페이스
사용자 공간 인터페이스에 대한 정의는 [README.md 파일](/README.md) 혹은 [switchtec_ioclt.h](/linux/switchtec_ioctl.h)를 참고하십시오.

커널 모듈은 열거된 각 스위치에 대한 문자 디바이스(character device)를 생성합니다.

이러한 디버이스에 읽고 쓰는것을 통해 메모리 맵 원격 함수 호출(Memory-mapped Remote Procedure Call, MRPC)을 통해 명령어를 전송할 수 있으며, 

미리 구현된 IOTCL을 통해 일반 주소 공간(GAS, Generic Address Space)에 직접 접근할 필요 없이 드라이버의 사용이 가능합니다. 

IOTCL을 사용하는것으로 일반 주소 공간에 직접 접근함으로써 발생하는 보안 및 안정성의 강화가 이루어지기 때문에 Switchtec 커널 모듈에 의존하는 어플리케이션에 큰 이점을 가져다 줍니다.

미리 구현된 IOTCL과 구현 방식은 [switchtec.c 파일의 switch_fops 변수](/switchtec.c)를 참고할 수 있습니다.


사용자 공간 어플리케이션이 Switchtec 문자 디바이스를 열 때마다, 커널은 switchtec_user 구조체를 생성합니다.

이 구조체는 MRPC 명령을 큐(Queue)하는데 사용되며, 커널은 해당 구조체를 통해 한번에 하나의 MRPC 명령이 이루어지게끔 중재합니다.

이러한 방식을 통해 각 응용 프로그램은 안정적으로 한번에 하나의 RPC 명령을 실행시킬 수 있습니다.


어플리케이션이 쓰기 작업을 수행하면, 커널은 펌웨어로 전송할 데이터를 대기열에 추가합니다.

만약 대기열이 비어있다면 커널은 즉시 명령어를 전송합니다. (해당 기능의 구현은 [mrpc_queue_cmd](/switchtec.c)를 참고하세요.)

읽기 작업은 얼마나 데이터를 읽어야 하는지 저장하고, 펌웨어가 지정된 데이터의 양만큼 반환할때 까지 대기합니다.

이벤트 인터럽트는 명령어가 처리된 후, 커널이 출력 데이터를 읽고 이를 switchtec_user 구조체에 저장할 때를 나타냅니다. (해당 기능의 구현은 [mrpc_complete_cmd](/switchtec.c)를 참고하세요.)

읽기 작업이 예정된 출력된 데이터의 양을 설정하지 않은 경우, 커널은 모든 출력 데이터를 버퍼로 읽어들입니다.

데이터를 모두 읽었다면 switchtec_user 구조체가 사용자 공간으로 데이터를 반환하기 위해 읽기 작업을 재개시킵니다.


예기치 못한 문제가 발생했을 경우를 대비하기 위해 커널은 모든 MRPC 명령어에 타임아웃을 적용합니다.(해당 기능의 구현은 [mrpc_timeout_work](/switchtec.c)를 참고하세요.)

일반적으로 언터럽트는 타임아웃이 적용되기 전에 발생하나, 인터럽트가 시간 초과로 발생하지 않았을 경우에도 대기열은 중단되지 않습니다.

하지만, 펌웨어에서 명령어 처리가 완료되었다고 표시하기 전에는 대기열이 재개되지 않기에 주의해야 합니다.

### Interrupts

The driver sets up space for up to four MSI-X or MSI interrupts but only
registers a handler for the event interrupt as designated by the
vep_vector_number in the GAS region. The NTB module will also register
another interrupt handler for the doorbell and message vector.

The event interrupt (switchtec_event_isr) first checks if the MRPC event
occurred and queues mrpc_work which will call mrpc_complete_cmd. It will
then clear the EVENT_OCCURRED bit so the interrupt doesn't continue to
trigger.

Next, the interrupt will check all the link state events in all the
ports and signal a link_notifier (typically used by the NTB driver)
if such an event occurs.

Finally, the interrupt will check all other event interrupts. If
an event interrupt occurs it wakes up any process that is polling
on events (see switchtec_dev_poll). It then disables the interrupt
for that event. In this way, it is expected that an application will
enable the interrupt it's waiting for, then call poll in a loop
checking for if the expected interrupt occurs. poll will return anytime
any event occurs.

### IOCTLs

A number of IOCTLs are provided for a number of functions needed by
switchtec-user. See the README for a description of these IOCTLs and
switchtec_dev_ioctl for their implementation.

### Sysfs

There are a number of sysfs attributes provided so that userspace can
easily enumerate and discover the available switchtec devices. The
attributes in the system can easily by browsed in sysfs under
/sys/class/switchtec.

These attributes are documented in Documentation/ABI/sysfs-class-switchtec.
See switchtec_device_attrs in switchtec.c for their implementation.

## ntb_hw_switchtec.ko

The ntb_hw_switchtec enumerates all devices in the switchtec class
and creates NTB interfaces for any devices that are NTB endpoints.
See switchtec_ntb_ops for the implementation of all the NTB operations.

### Shared Memory Window

The Switchtec NTB driver reserves one of the LUT memory windows so it
can be used to provide scratch pad registers and link detection. For
now, the driver sets the size of all LUT windows to be fixed at 64KB.
This size allows for the combined size of all LUT windows to be
sufficent enough that the alignment of the direct window that follows
will be at least 2MB.

### Link Management

The link is considered to be up when both sides have setup their shared
memory window and a magic number and link status must be read by both
sides to realize that the link is up. When either side changes their
link status, a specific message is sent telling the otherside to check
the current link state. The link state is also checked whenever the
switch sends a link state change interrupt.

### Memory windows

By default, the driver only provides direct memory windows to the
upper layers. This is because the existing upper layers can get confused
by a large number of LUT memory windows. The LUT memory windows can be
enabled with the use_lut_mws parameter.

### Crosslink

The crosslink feature allows for an NTB system to be entirely symmetric
such that two hosts can be identical and interchangeable. To do this a
special hostless partition is created in the middle of the two hosts.
This is supported by the driver and only requires a special initialization
procedure (see switchtec_ntb_init_crosslink). Crosslink also reserves another
one of the LUT windows to be used to window the NTB register space inside
the crosslink partition. Besides this, all other NTB operations function
identically to regular NTB.

[1]: https://lwn.net/Kernel/LDD3/
