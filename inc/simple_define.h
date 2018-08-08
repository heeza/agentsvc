#ifndef __SIMPLE_DEFINE_H_INCLUDED__
#define __SIMPLE_DEFINE_H_INCLUDED__

#define STATE_A		1
#define STATE_B		2
#define STATE_C		3
#define STATE_D		4
#define STATE_E		5
#define STATE_F		6
#define STATE_G		7
#define STATE_H		8
#define STATE_I		9
#define STATE_J		10
#define STATE_K		11
#define STATE_L		12
#define STATE_M		13
#define STATE_N		14
#define STATE_O		15
#define STATE_P		16
#define STATE_Q		17
#define STATE_R		18
#define STATE_S		19
#define STATE_T		20
#define STATE_U		21
#define STATE_V		22
#define STATE_W		23
#define STATE_X		24
#define STATE_Y		25
#define STATE_Z		26


#define OK		0
#define NG		1

#define T_LOGIN		0x0001
#define T_DATA		0x0002
#define T_IMGFACE		0x0004
#define T_POLL		0x0008
#define T_IMGCARD		0x0010
#define T_TROUBLE		0x0020
#define T_DREQ		0x0040
#define T_DCOMP		0x0080
#define T_DINFO		0x0100
#define T_DDATA		0x0200
#define T_FILEREQ		0x0400
#define T_FIELVER		0x8000


#define NM_MSG_OK	"00"
#define NM_MSG_NG	"01"
#define NM_MSG_FA	"02"

#define IDX_RESULT	21

#define REQ_IMG		0x0010	// 이미지 요청
#define REQ_FIL		0x0020	// 파일 요청
#define REQ_CHG		0x0040	// 운영모드 변경 요청
#define REQ_RST		0x0080	// 원격리셋 요청
#define REQ_RSP		0x0100	// 원격응답요청 대기
#define REQ_RCM		0x0200	// 이미지 전송 모드 변경
#define REQ_JCM		0x0400	// 이미지 & 저널 운영 모드 변경
#define REQ_DWN		0x1000	// 파일 다운로드 실행할것...

#define REQ_REV		0x0800	// 자동화기기로 부터 현재 상태 알아옴

#define F_ETC		1
#define F_COM		0

#define WZ_SUCCESS	7
#define WZ_FAILED	8
#define WZ_CONTINUE	9

#define WZ_IDLE		0	// 고객 거래 대기중
#define WZ_SERVICE	1	// 기기 거래모드 중(기기 고객 거래중)
#define WZ_POWERUP	2	// 기기 초기모드 중(전원 ON 후 MECHA등 초기화 중)
#define WZ_OFFLINE	3	// 기기 개국모드 중(개국시도중)
#define WZ_MFAULT	4	// 기기 장애모드 중
#define WZ_MANUAL	5	// 기기 계원모드 중
#define WZ_ETCMODE	6	// 상위 모드 外

#endif // __SIMPLE_DEFINE_H_INCLUDED__