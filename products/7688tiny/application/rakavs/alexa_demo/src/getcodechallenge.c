#include <stdio.h>
#include <string.h>
#include <math.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "getcodechallenge.h"

#ifndef __FILE_NAME_SAFE_ALPHA__
#define __FILE_NAME_SAFE_ALPHA__  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~"
#endif
#define BASE64URLMAXINDEX 66
int Base64UrlEncode( const char * in, int inlen, char * out )
{
    int outlen = EVP_EncodeBlock((unsigned char *)out, (unsigned char *)in, inlen);
    inlen = outlen;//because inlen is not useful, we use inlen to store the value of outlen.
    while(outlen){
        --outlen;//when outlen==1，next outlen==0;
        switch(*(out+outlen)){
            case '+': 
                *(out+outlen)='-';
                break;
            case '/': 
                *(out+outlen)='_';
                break;
            case '=':
                *(out+outlen)='\0';
                --inlen;
                break;
        }
    }
    return inlen;
}
int generateCodeVerifier(unsigned char * ucCodeVerifier)
{
	while( !RAND_status() );
	unsigned char ucRandomUcharArray[ CODE_VERIFIER_MAX_LEN] = {0};
	int iRandomUcharArrayLen, ret;
	{
		unsigned char len;
		if(!RAND_bytes( &len, 1 )){//here happens "Segmentation fault",if ues randomUcharArrayLen to replace len.
			if(!RAND_bytes( &len, 1 )){
				fprintf(stderr,"generate CodeVerifierLen Err:\n%s\n", ERR_error_string(ERR_get_error(), NULL));
				return -1;
			}
		}
		iRandomUcharArrayLen = (int)len % 81 + 45;
	}
	
	if( !RAND_bytes( ucRandomUcharArray, iRandomUcharArrayLen )){
		while( !RAND_status() );
		if( !RAND_bytes( ucRandomUcharArray, iRandomUcharArrayLen )  ){
			fprintf(stderr,"generate CodeVerifier Err:\n%s\n", ERR_error_string(ERR_get_error(), NULL));
			return -1;
		}
	}
	ret = iRandomUcharArrayLen;
	ucCodeVerifier[iRandomUcharArrayLen] = 0;
	char * strsrcalpha = __FILE_NAME_SAFE_ALPHA__;
	for(iRandomUcharArrayLen--;iRandomUcharArrayLen>=0;iRandomUcharArrayLen--){
		ucCodeVerifier[iRandomUcharArrayLen] = strsrcalpha[ucRandomUcharArray[iRandomUcharArrayLen] % BASE64URLMAXINDEX];
	}
	return ret;
}
int generateCodeChallenge(const unsigned char* ucCodeVerifier, const int iCodeVerifierLen, unsigned char* ucCodeChallenge)
{
	unsigned char sha256[SHA256_DIGEST_LENGTH];
	unsigned char *sha = SHA256(ucCodeVerifier, iCodeVerifierLen, sha256);
	if(sha == sha256)
	    return Base64UrlEncode( (const char *)sha256, SHA256_DIGEST_LENGTH, (char *)ucCodeChallenge );
	return -1;
	
}
#undef BASE64URLMAXINDEX
#ifdef __FILE_NAME_SAFE_ALPHA__
#undef __FILE_NAME_SAFE_ALPHA__
#endif
