#ifndef  __GETCODECHALLENGE_H__
#define  __GETCODECHALLENGE_H__

#define CODE_VERIFIER_MAX_LEN 129
#define CODE_CHALLENGE_LEN    45

int Base64UrlEncode( const char * in, int inlen, char * out );
int generateCodeVerifier(unsigned char * ucCodeVerifier);
int generateCodeChallenge(const unsigned char* ucCodeVerifier, const int iCodeVerifierLen, unsigned char* ucCodeChallenge);

#endif

