


#define ERROR_LEN 1024

/*
#if defined(__cplusplus)
extern "C" {
#endif
*/

/************************************************************************/
/* JPG ������ ������ JP2�������� ��ȯ �մϴ�.
	sInFile : JPG���� ��
	sOutFile: ������ JP2���� ��
	sRate	: ����� (������� �������� ���ϻ���� �۴�.)
			- 0.01 ~ 10 ���� ��밡����. 
			- 1.0 ~ 2.0 ���̸� ������.
	
																		*/
/************************************************************************/
int WINAPI vis_jp2_encoder(const char *sInFile, const char *sOutFile, const char *sRate);


/************************************************************************/
/* vis_jp2_encoder��⿡�� ������ �� ��� ���� �������� �����´�.
	szErr	: char�� �迭�� ERROR_LEN��ŭ �����Ͽ� �ѱ⵵�� �Ѵ�.
                                                                     */
/************************************************************************/
void WINAPI vis_jp2_encoder_getlasterror(char *szErr);


/*
#if defined(__cplusplus)
}
#endif
*/