#define ERROR_LEN 1024


#if defined(__cplusplus)
extern "C" {
#endif


/************************************************************************/
/* JP2 ������ ������ JPG�������� ��ȯ �մϴ�.
	sInFile : JP2���� ��
	sOutFile: ������ JPG���� ��
	
																		*/
/************************************************************************/
int WINAPI vis_jp2_decoder(const char *sInFile, const char *sOutFile);


/************************************************************************/
/* vis_jp2_decoder��⿡�� ������ �� ��� ���� �������� �����´�.
	szErr	: char�� �迭�� ERROR_LEN��ŭ �����Ͽ� �ѱ⵵�� �Ѵ�.
                                                                     */
/************************************************************************/
void WINAPI vis_jp2_decoder_getlasterror(char *szErr);

#if defined(__cplusplus)
}
#endif
