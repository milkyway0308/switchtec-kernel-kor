# Linux Switchtec Support - 한글 번역!

Microsemi의 "Switchtec" 계열의 PCI 스위치 디바이스는 이미 표준 PCI 스위치 디바이스에서 지원되고 있습니다.

하지만, Switchtec 기기는 몇가지 특별한 관리 엔드포인트로 추가적인 기능을 활성화할 수 있는것으로 알려져 있습니다.

추가 기능으로 다음을 포함합니다 :
- 패킷과 바이트 카운터
- 펌웨어 업데이트
- 오류와 이벤트 로그
- 포트 링크 상태 쿼리
- 맟춤 사용자 펌웨어 명령어

Switchtec Kernel은 이러한 추가 기능을 사용 가능하게 구현되었습니다.

### 인터페이스

Switchtec 관리 펌웨어와 통신한다는것은 메모리 맵 원격 함수 호출(Memory-mapped Remote Procedure Call, MRPC)를 통해 통신한다는것을 의미합니다.

명령어는 4 바이트의 커맨드 아이디(Command ID)와 최대 1KB의 명령어 별 데이터(Command Specific Data)를 인터페이스에 전달함으로써 작동합니다.

펌웨어는 명령어를 수신한 후 후, 4 바이트의 반환 코드 인식자와 최대 1KB의 명령어 별 데이터를 반환합니다.

인터페이스는 한번에 1개의 명령어만을 처리할 수 있습니다.


### 사용자 공간 인터페이스 (Userspace Interface)

MRPC 인터페이스는 각 관리 엔드포인트마다 하나씩 간단한 문자 디바이스(Char Device)를 통해 `dev/switchtec#`과 같은 이름으로 사용자 공간에 노출됩니다.

문자 디바이스는 다음과 같은 규칙을 따릅니다 :

* 쓰기 작업(write)은 반드시 최소 4 바이트, 최대 1028 바이트로 구성되어야 합니다.
  첫 4 바이트는 커맨드 아이디로 해석되며, 나머지 데이터는 커맨드 별 데이터로 인식됩니다.
  쓰기 작업은 처리를 시작하기 위해 명령어를 펌웨어에 전송할 것입니다.


* 각 쓰기 작업마다 정확히 1개의 읽기 작업(read)이 뒤따라와야 합니다.
  2번 이상의 중복된 쓰기 작업은 오류를 발생시키며,
  쓰기 작업 없이 읽기 작업을 하는것 또한 오류를 발생시킵니다.


* 읽기 작업은 펌웨어가 명령어 처리를 완료하고,
  4 바이트 반환 코드와 최대 1024 바이트의 출력 데이터를 반환할때까지 블로킹(blocking)됩니다.
  (출력 데이터의 길이는 읽기 작업의 size 인수입니다.
  4 바이트보다 적게 읽는다면 오류가 발생합니다.)


* 펌웨어의 명령어 처리 동안 다른 작업을 수행해야 하는 사용자 공간 어플리케이션을 위해 폴링 요청 또한 지원됩니다.


다음 IOTCL 또한 디바이스에서 지원됩니다 :


* SWITCHTEC_IOCTL_FLASH_INFO - 해당 디바이스에 존재하는 펌웨어의 길이와 개수를 검색합니다.


* SWITCHTEC_IOCTL_FLASH_PART_INFO - 플래시 메모리에 존재하는 특정 파티션의 주소와 길이를 검색합니다.


* SWITCHTEC_IOCTL_EVENT_SUMMARY - 모든 해제되지 않은 이벤트를 나타내는 비트맵 구조체를 읽습니다.


* SWITCHTEC_IOCTL_EVENT_CTL - 이벤트가 발생한 횟수를 가져오거나, 모든 이벤트에 대해 플래그를 설정하거나 제거합니다.

  해당 명령은 인자로 switchtec_ioctl_event_ctl 구조체를 사용하며,

  해당 구조체에는 event_id와 index, 그리고 flags 값이 설정되어야 합니다.

  (index 값은 전역 이벤트가 아닐 경우 파티션 혹은 PFF 번호를 사용합니다.)

  해당 명령은 이벤트 발생 여부와 발생 횟수, 그리고 이벤트의 특정 데이터를 반환합니다.

  인자에 flags 값을 어떻게 설정하냐에 따라 이벤트 발생 횟수를 초기화하거나,

  이벤트가 발생할 때 실행되는 작업을 활성화하거나 비활성화시킬 수 있습니다.

  SWITCHTEC_IOCTL_EVENT_FLAG_EN_POLL 플래그를 사용하면, 해당 이벤트가 발생할 때
  poll 명령어를 작동시켜 POLLPRI로 반환하게끔 할 수 있습니다.

  이러한 방법을 사용함으로써 사용자 공간에서 이벤트가 발생할 때까지 대기할 수 있습니다.


* SWITCHTEC_IOCTL_PFF_TO_PORT - PCI 함수 프레임워크(PCI Function Framework, PFF) 번호를 Switchtec 로직 포트 ID(Switchtec Logic Port ID)와 파티션 번호로 변환합니다.


* SWITCHTEC_IOCTL_PORT_TO_PFF - 파티션 번호와 SwitchTec 로직 포트 ID를 PCI 함수 프레임워크 번호로 변환합니다.


Non-Transparent Bridge (NTB) 드라이버
===================================

NTB 하드웨어 드라이버는 Switchtec 하드웨어 지원을 위해 ntb_hw_switchtec 파일에 작성되었습니다.
현재로써는 정확히 2개의 NT 파티션과 0개 이상의 비-NT 파티션으로 이루어진 스위치만을 지원합니다.
또한, NTB 드라이버는 다음 구성 설정이 필요합니다 :

* 각 NT 파티션은 서로의 일반 주소 공간(GAS, Generic Address Space)에 접근할 수 있어야 합니다.
  그러므로, 관리 설정에서 일반 주소 공간 액세스 비트가 이를 지원하도록 설정되어야 합니다.


* 커널 설정이 반드시 NTB를 지원하도록 설정되어야 합니다.
  (CONFIG_NTB를 설정해야 함)

NT EP BAR 2(Non-Transparent Endpoint Base Address Register 2)은 Direct Window로 동적으로 구성되므로, 구성 파일에서는 이를
명시적으로 구성할 필요가 없습니다.

Linux NTB stack에 대한 전반적인 이해는 Linux 소스 트리의 Documentation/ntb.txt를 참고하십시오.
ntb_hw_switchtec은 해당 스택에서 NTB 하드웨어 드라이브로 작동합니다.