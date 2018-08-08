


#define ERROR_LEN 1024

/*
#if defined(__cplusplus)
extern "C" {
#endif
*/

/************************************************************************/
/* JPG 포멧의 파일을 JP2포멧으로 변환 합니다.
	sInFile : JPG파일 명
	sOutFile: 생성될 JP2파일 명
	sRate	: 압축률 (압축률이 낮을수록 파일사이즈가 작다.)
			- 0.01 ~ 10 까지 사용가능함. 
			- 1.0 ~ 2.0 사이를 권장함.
	
																		*/
/************************************************************************/
int WINAPI vis_jp2_encoder(const char *sInFile, const char *sOutFile, const char *sRate);


/************************************************************************/
/* vis_jp2_encoder모듈에서 오류가 난 경우 오류 메지지를 가져온다.
	szErr	: char형 배열을 ERROR_LEN만큼 생성하여 넘기도록 한다.
                                                                     */
/************************************************************************/
void WINAPI vis_jp2_encoder_getlasterror(char *szErr);


/*
#if defined(__cplusplus)
}
#endif
*/