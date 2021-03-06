// verifier.cpp - written and placed in the public domain by Wei Dai
// g++ -o verifier verifier.cpp libcryptopp.a

#include "dll.h"
#include "pch.h"

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include "rsa.h"
#include "md2.h"
#include "nr.h"
#include "dsa.h"
#include "dh.h"
#include "mqv.h"
#include "luc.h"
#include "xtrcrypt.h"
#include "rabin.h"
#include "rw.h"
#include "asn.h"
#include "rng.h"
#include "files.h"
#include "hex.h"
#include "oids.h"
#include "esign.h"
#include "osrng.h"

#include "md5.h"
#include "ripemd.h"
#include "rng.h"
#include "modes.h"
#include "randpool.h"
#include "ida.h"
#include "base64.h"
#include "factory.h"

#include "regtest.cpp"

#include "bench.h"

#include <iostream>
#include <iomanip>
#include <ctime>
#include <cassert>
#include <chrono>

USING_NAMESPACE(CryptoPP)
USING_NAMESPACE(std)

// The following website contains the mapping of security levels to the appropriate scheme parameters
// http://www.cryptopp.com/wiki/Security_Level
#define NUMBER_OF_SECURITY_LENGTHS 5
int securityLevels[NUMBER_OF_SECURITY_LENGTHS] = {80, 112, 128, 192, 256};
int finiteFieldSizes[NUMBER_OF_SECURITY_LENGTHS] = {1024, 2048, 3072, 7680, 15360};
int finiteFieldSubgroupSizes[NUMBER_OF_SECURITY_LENGTHS] = {160, 224, 256, 384, 511};
int factorizationGroupSizes[NUMBER_OF_SECURITY_LENGTHS] = {1024, 2048, 3072, 7680, 15360};
int ellipticCurveSizes[NUMBER_OF_SECURITY_LENGTHS] = {160, 224, 256, 384, 512};

static OFB_Mode<AES>::Encryption s_globalRNG;

RandomNumberGenerator & GlobalRNG()
{
	return s_globalRNG;
}

string generateDetailedDescription(const string algorithmName,
	const int securityLevel, const int keyLength, const int inputLength) {
	string fullDescription;
	fullDescription.append(algorithmName);
	fullDescription.append(",");
	fullDescription.append(to_string(securityLevel));
	fullDescription.append(",");
	fullDescription.append(to_string(keyLength));
	fullDescription.append(",");
	fullDescription.append(to_string(inputLength));
	return fullDescription;
}

string 
generateSignDescription(const string algorithmName, const int securityLevel, const int keyLength, const int inputLength) {
	string fullDescription;
	fullDescription.append("sign,");
	fullDescription.append(generateDetailedDescription(algorithmName, securityLevel, keyLength, inputLength));
	return fullDescription;
}

string 
generateVerifyDescription(const string algorithmName, const int securityLevel, const int keyLength, const int inputLength) {
	string fullDescription;
	fullDescription.append("verify,");
	fullDescription.append(generateDetailedDescription(algorithmName, securityLevel, keyLength, inputLength));
	return fullDescription;
}

string 
generateCSVString(string description, string operation, size_t delta) {
	string csv;
	csv.append(description);
	csv.append(",");
	csv.append(operation);
	csv.append(",");
	csv.append(to_string(delta));
	return csv;
}

class FixedRNG : public RandomNumberGenerator
{
public:
	FixedRNG(BufferedTransformation &source) : m_source(source) {}

	void GenerateBlock(byte *output, size_t size)
	{
		m_source.Get(output, size);
	}

private:
	BufferedTransformation &m_source;
};

bool ProfileSignatureValidate(PK_Signer &priv, PK_Verifier &pub, const byte *input, 
	const size_t inputLength, string description, bool thorough = false)
{
	bool pass = true, fail;

	fail = !pub.GetMaterial().Validate(GlobalRNG(), thorough ? 3 : 2) || !priv.GetMaterial().Validate(GlobalRNG(), thorough ? 3 : 2);
	assert(pass && !fail);

	SecByteBlock signature(priv.MaxSignatureLength());

	std::chrono::steady_clock::time_point signStartTime = std::chrono::steady_clock::now();
	size_t signatureLength = priv.SignMessage(GlobalRNG(), input, inputLength, signature);
	std::chrono::steady_clock::time_point signEndTime = std::chrono::steady_clock::now();
	size_t signNanoSeconds = std::chrono::duration_cast<std::chrono::nanoseconds>(signEndTime - signStartTime).count();

	cout << generateCSVString(description, "sign", signNanoSeconds) << endl;

	std::chrono::steady_clock::time_point verifyStartTime = std::chrono::steady_clock::now();
	fail = !pub.VerifyMessage(input, inputLength, signature, signatureLength);
	std::chrono::steady_clock::time_point verifyEndTime = std::chrono::steady_clock::now();
	size_t verifyNanoSeconds = std::chrono::duration_cast<std::chrono::nanoseconds>(verifyEndTime - verifyStartTime).count();

	cout << generateCSVString(description, "verify", verifyNanoSeconds) << endl;

	assert(pass && !fail);
	return pass;
}

bool ValidateRSA(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("RSA", securityLevels[secLevelIndex], 
		factorizationGroupSizes[secLevelIndex], inputLength);

	// FileSource keys("TestData/rsa512a.dat", true, new HexDecoder);
	// FileSource keys("mykey.pem", true, new HexDecoder);

	// Weak::RSASSA_PKCS1v15_MD2_Signer rsaPriv(keys);

	Weak::RSASSA_PKCS1v15_MD2_Signer rsaPriv(GlobalRNG(), factorizationGroupSizes[secLevelIndex]);
	Weak::RSASSA_PKCS1v15_MD2_Verifier rsaPub(rsaPriv);

	bool pass = ProfileSignatureValidate(rsaPriv, rsaPub, input, inputLength, description);
	assert(pass);

	return pass;
}

bool ValidateNR(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("NR", securityLevels[secLevelIndex], 
		factorizationGroupSizes[secLevelIndex], inputLength);

	NR<SHA>::Signer privS(GlobalRNG(), finiteFieldSubgroupSizes[secLevelIndex]);
	privS.AccessKey().Precompute();
	NR<SHA>::Verifier pubS(privS);

	bool pass = ProfileSignatureValidate(privS, pubS, input, inputLength, description);
	assert(pass);
	return pass;
}

bool ValidateDSA(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("DSA", securityLevels[secLevelIndex], 
		factorizationGroupSizes[secLevelIndex], inputLength);

	DSA::Signer priv(GlobalRNG(), factorizationGroupSizes[secLevelIndex]);
	DSA::Verifier pub(priv);
	bool pass = ProfileSignatureValidate(priv, pub, input, inputLength, description);
	assert(pass);

	return pass;
}

bool ValidateLUC(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("LUC", securityLevels[secLevelIndex],
		factorizationGroupSizes[secLevelIndex], inputLength);

	LUCSSA_PKCS1v15_SHA_Signer priv(GlobalRNG(), factorizationGroupSizes[secLevelIndex]);
	LUCSSA_PKCS1v15_SHA_Verifier pub(priv);
	bool pass = ProfileSignatureValidate(priv, pub, input, inputLength, description);
	assert(pass);

	return pass;
}

bool ValidateLUC_DL(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("LUC-DL", securityLevels[secLevelIndex], 
		finiteFieldSizes[secLevelIndex], inputLength);

	LUC_HMP<SHA>::Signer privS(GlobalRNG(), finiteFieldSizes[secLevelIndex]);
	LUC_HMP<SHA>::Verifier pubS(privS);
	bool pass = ProfileSignatureValidate(privS, pubS, input, inputLength, description);
	assert(pass);

	return pass;
}

bool ValidateRabin(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("Rabin", securityLevels[secLevelIndex], 
		factorizationGroupSizes[secLevelIndex], inputLength);

	RabinSS<PSSR, SHA>::Signer priv(GlobalRNG(), factorizationGroupSizes[secLevelIndex]);
	RabinSS<PSSR, SHA>::Verifier pub(priv);
	bool pass = ProfileSignatureValidate(priv, pub, input, inputLength, description);
	assert(pass);

	return pass;
}

bool ValidateRW(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("RW", securityLevels[secLevelIndex], 
		factorizationGroupSizes[secLevelIndex], inputLength);

	RWSS<PSSR, SHA>::Signer priv(GlobalRNG(), factorizationGroupSizes[secLevelIndex]);
	RWSS<PSSR, SHA>::Verifier pub(priv);
	bool pass = ProfileSignatureValidate(priv, pub, input, inputLength, description);
	assert(pass);

	return pass;
}

bool ValidateECDSA(const byte *input, const size_t inputLength, const int secLevelIndex)
{
	string description = generateDetailedDescription("ECDSA", securityLevels[secLevelIndex], 1, inputLength);

	// from Sample Test Vectors for P1363
	GF2NT gf2n(191, 9, 0);
	byte a[]="\x28\x66\x53\x7B\x67\x67\x52\x63\x6A\x68\xF5\x65\x54\xE1\x26\x40\x27\x6B\x64\x9E\xF7\x52\x62\x67";
	byte b[]="\x2E\x45\xEF\x57\x1F\x00\x78\x6F\x67\xB0\x08\x1B\x94\x95\xA3\xD9\x54\x62\xF5\xDE\x0A\xA1\x85\xEC";
	EC2N ec(gf2n, PolynomialMod2(a,24), PolynomialMod2(b,24));

	EC2N::Point P;
	ec.DecodePoint(P, (byte *)"\x04\x36\xB3\xDA\xF8\xA2\x32\x06\xF9\xC4\xF2\x99\xD7\xB2\x1A\x9C\x36\x91\x37\xF2\xC8\x4A\xE1\xAA\x0D"
		"\x76\x5B\xE7\x34\x33\xB3\xF9\x5E\x33\x29\x32\xE7\x0E\xA2\x45\xCA\x24\x18\xEA\x0E\xF9\x80\x18\xFB", ec.EncodedPointSize());
	Integer n("40000000000000000000000004a20e90c39067c893bbb9a5H");
	Integer d("340562e1dda332f9d2aec168249b5696ee39d0ed4d03760fH");
	EC2N::Point Q(ec.Multiply(d, P));
	ECDSA<EC2N, SHA>::Signer priv(ec, P, n, d);
	ECDSA<EC2N, SHA>::Verifier pub(priv);

	bool pass = ProfileSignatureValidate(priv, pub, input, inputLength, description);
	assert(pass);

	return pass;
}

// bool ValidateESIGN(const byte *input, const size_t inputLength, const int secLevelIndex)
// {
// 	string description = generateDetailedDescription("ESIGN", securityLevels[secLevelIndex], 1);
// 
// 	FileSource keys("TestData/esig1536.dat", true, new HexDecoder);
// 	ESIGN<SHA>::Signer signer(keys);
// 	ESIGN<SHA>::Verifier verifier(signer);
// 
// 	bool pass = ProfileSignatureValidate(signer, verifier, input, inputLength, description);
// 	assert(pass);
// 
// 	return pass;
// }

void ProfileSignatureSchemes(const byte *inputData, const size_t inputLength, const int securityLevel) {
	ValidateRSA(inputData, inputLength, securityLevel);
	ValidateNR(inputData, inputLength, securityLevel);
	ValidateDSA(inputData, inputLength, securityLevel);
	ValidateLUC(inputData, inputLength, securityLevel);
	ValidateLUC_DL(inputData, inputLength, securityLevel);
	ValidateRabin(inputData, inputLength, securityLevel);
	ValidateRW(inputData, inputLength, securityLevel);
	ValidateECDSA(inputData, inputLength, securityLevel);
}

void showUsage() {
	cout << "usage: verifier <security-level> <rng-seed>" << endl;
	cout << "       security-level: the AES security equivalent level" << endl;
	cout << "       rng-seed:       the seed for the global RNG" << endl;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		showUsage();
		return 1;
	}

	int securityLevel = atoi(argv[1]);
	string rngSeed(argv[2]);
	size_t rngSeedLength = 16;

	string fullLine;
	string line;
	while (getline(cin, line)) {
		fullLine.append(line);
	}
	byte *inputData = (byte *) fullLine.data();
	int inputLength = fullLine.length();

	RegisterFactories();
	rngSeed.resize(rngSeedLength);
	s_globalRNG.SetKeyWithIV((byte *)rngSeed.data(), rngSeedLength, (byte *)rngSeed.data());

	int securityIndex = 0;
	for (int i = 0; i < NUMBER_OF_SECURITY_LENGTHS; i++) {
		if (securityLevels[i] == securityLevel) {
			securityIndex = i;
			break;
		}
	}
	
	ProfileSignatureSchemes(inputData, inputLength, securityIndex);
}
