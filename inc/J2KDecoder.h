#define ERROR_LEN 1024


#if defined(__cplusplus)
extern "C" {
#endif


/************************************************************************/
/* JP2 포멧의 파일을 JPG포멧으로 변환 합니다.
	sInFile : JP2파일 명
	sOutFile: 생성될 JPG파일 명
	
																		*/
/************************************************************************/
int WINAPI vis_jp2_decoder(const char *sInFile, const char *sOutFile);


/************************************************************************/
/* vis_jp2_decoder모듈에서 오류가 난 경우 오류 메지지를 가져온다.
	szErr	: char형 배열을 ERROR_LEN만큼 생성하여 넘기도록 한다.
                                                                     */
/************************************************************************/
void WINAPI vis_jp2_decoder_getlasterror(char *szErr);

#if defined(__cplusplus)
}
#endif
