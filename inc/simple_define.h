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

#define REQ_IMG		0x0010	// �̹��� ��û
#define REQ_FIL		0x0020	// ���� ��û
#define REQ_CHG		0x0040	// ���� ���� ��û
#define REQ_RST		0x0080	// ���ݸ��� ��û
#define REQ_RSP		0x0100	// ���������û ���
#define REQ_RCM		0x0200	// �̹��� ���� ��� ����
#define REQ_JCM		0x0400	// �̹��� & ���� � ��� ����
#define REQ_DWN		0x1000	// ���� �ٿ�ε� �����Ұ�...

#define REQ_REV		0x0800	// �ڵ�ȭ���� ���� ���� ���� �˾ƿ�

#define F_ETC		1
#define F_COM		0

#define WZ_SUCCESS	7
#define WZ_FAILED	8
#define WZ_CONTINUE	9

#define WZ_IDLE		0	// �� �ŷ� �����
#define WZ_SERVICE	1	// ��� �ŷ���� ��(��� �� �ŷ���)
#define WZ_POWERUP	2	// ��� �ʱ��� ��(���� ON �� MECHA�� �ʱ�ȭ ��)
#define WZ_OFFLINE	3	// ��� ������� ��(�����õ���)
#define WZ_MFAULT	4	// ��� ��ָ�� ��
#define WZ_MANUAL	5	// ��� ������ ��
#define WZ_ETCMODE	6	// ���� ��� ��

#endif // __SIMPLE_DEFINE_H_INCLUDED__