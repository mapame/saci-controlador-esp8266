#define TAs_NUM   1

/* Maquiatto */
/* Obtained in 20/10/2020 */
/*
static const unsigned char TA0_DN[] = {
	0x30, 0x5C, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
	0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08,
	0x0C, 0x0A, 0x53, 0x6F, 0x6D, 0x65, 0x2D, 0x53, 0x74, 0x61, 0x74, 0x65,
	0x31, 0x21, 0x30, 0x1F, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x0C, 0x18, 0x49,
	0x6E, 0x74, 0x65, 0x72, 0x6E, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67,
	0x69, 0x74, 0x73, 0x20, 0x50, 0x74, 0x79, 0x20, 0x4C, 0x74, 0x64, 0x31,
	0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0C, 0x0C, 0x6D, 0x61,
	0x71, 0x69, 0x61, 0x74, 0x74, 0x6F, 0x2E, 0x63, 0x6F, 0x6D
};

static const unsigned char TA0_RSA_N[] = {
	0xF4, 0x14, 0xC4, 0x63, 0xC0, 0x70, 0x28, 0x80, 0x84, 0x80, 0xEF, 0x35,
	0x61, 0xA1, 0xD9, 0x41, 0x61, 0xB5, 0x50, 0x04, 0xBB, 0xD2, 0x62, 0x69,
	0xD5, 0x91, 0xCC, 0x13, 0xDA, 0x5A, 0x0C, 0x1E, 0x3E, 0xF1, 0x5F, 0x1E,
	0xD3, 0x4D, 0x8A, 0xC8, 0xAA, 0x2E, 0x93, 0xC1, 0xDF, 0xDF, 0x88, 0xB0,
	0xE1, 0x1A, 0xC9, 0xAF, 0x56, 0xEA, 0x2C, 0xE9, 0x17, 0xD7, 0x4D, 0x74,
	0x4B, 0x0B, 0x65, 0x7D, 0x28, 0x28, 0xAB, 0xF5, 0x27, 0x30, 0x58, 0x16,
	0xE0, 0x6C, 0x91, 0x03, 0x9B, 0xE2, 0x82, 0xEA, 0x10, 0xEC, 0xED, 0x9F,
	0x17, 0xFB, 0x8E, 0xAD, 0x79, 0x99, 0x84, 0xE9, 0x8A, 0x6F, 0xFA, 0xAC,
	0xBE, 0x9D, 0xE1, 0x5C, 0xF0, 0x7C, 0x70, 0xC7, 0xD3, 0x64, 0xF1, 0x06,
	0x03, 0x3C, 0x71, 0x53, 0xCF, 0x24, 0x88, 0xCD, 0xF2, 0xA7, 0xF2, 0x99,
	0x65, 0xC1, 0xD5, 0x16, 0xFC, 0xA6, 0x14, 0xFA, 0xAB, 0x5C, 0x13, 0xF2,
	0xD7, 0xB2, 0x4E, 0x03, 0x87, 0xC2, 0x6F, 0x01, 0x98, 0x29, 0x3E, 0xED,
	0x45, 0xFA, 0x8F, 0x98, 0x13, 0xF4, 0xB0, 0x79, 0x9C, 0x30, 0x99, 0x3A,
	0x68, 0xDC, 0x2B, 0xD9, 0xBF, 0xC1, 0x81, 0x3D, 0xC5, 0x00, 0x66, 0xC5,
	0xEB, 0xC6, 0x6D, 0x89, 0x45, 0xB7, 0x9C, 0x28, 0x5E, 0x3A, 0x44, 0xF8,
	0x14, 0xDA, 0x0D, 0xE5, 0xD5, 0x64, 0xCB, 0xA6, 0xE6, 0x0A, 0xA4, 0x27,
	0x50, 0xAF, 0xBE, 0x2C, 0x28, 0x36, 0x41, 0x5B, 0x01, 0x42, 0x79, 0x7B,
	0xAD, 0x7D, 0x80, 0x62, 0x5F, 0x2F, 0x61, 0xCE, 0xAD, 0xEE, 0xDC, 0x90,
	0x73, 0xCB, 0x46, 0x3A, 0x65, 0x09, 0x66, 0x9C, 0x35, 0xBE, 0x9E, 0x1D,
	0xED, 0xD1, 0xD2, 0xD6, 0x45, 0x35, 0xA0, 0xE9, 0x84, 0x62, 0xFC, 0x87,
	0x25, 0x27, 0x3C, 0xAF, 0x82, 0x9C, 0x51, 0xBE, 0x07, 0x87, 0x7B, 0x26,
	0xA2, 0xA5, 0x8A, 0xAF
};

static const unsigned char TA0_RSA_E[] = {
	0x01, 0x00, 0x01
};

static const br_x509_trust_anchor TAs[1] = {
	{
		{ (unsigned char *)TA0_DN, sizeof TA0_DN },
		0,
		{
			BR_KEYTYPE_RSA,
			{ .rsa = {
				(unsigned char *)TA0_RSA_N, sizeof TA0_RSA_N,
				(unsigned char *)TA0_RSA_E, sizeof TA0_RSA_E,
			} }
		}
	}
};
*/

/* adafruit.io */
/* Obtained in 20/10/2020 */

static const unsigned char TA0_DN[] = {
	0x30, 0x6E, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
	0x02, 0x55, 0x53, 0x31, 0x11, 0x30, 0x0F, 0x06, 0x03, 0x55, 0x04, 0x08,
	0x13, 0x08, 0x4E, 0x65, 0x77, 0x20, 0x59, 0x6F, 0x72, 0x6B, 0x31, 0x11,
	0x30, 0x0F, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13, 0x08, 0x4E, 0x65, 0x77,
	0x20, 0x59, 0x6F, 0x72, 0x6B, 0x31, 0x20, 0x30, 0x1E, 0x06, 0x03, 0x55,
	0x04, 0x0A, 0x13, 0x17, 0x41, 0x64, 0x61, 0x66, 0x72, 0x75, 0x69, 0x74,
	0x20, 0x49, 0x6E, 0x64, 0x75, 0x73, 0x74, 0x72, 0x69, 0x65, 0x73, 0x20,
	0x4C, 0x4C, 0x43, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04, 0x03,
	0x0C, 0x0E, 0x2A, 0x2E, 0x61, 0x64, 0x61, 0x66, 0x72, 0x75, 0x69, 0x74,
	0x2E, 0x63, 0x6F, 0x6D
};

static const unsigned char TA0_RSA_N[] = {
	0xC6, 0xD6, 0xF2, 0x95, 0xED, 0xAC, 0x29, 0x3D, 0x46, 0x91, 0xDD, 0xB4,
	0x74, 0x5F, 0x2B, 0x1E, 0xC0, 0x81, 0x08, 0xC7, 0xE8, 0xA3, 0x1A, 0x95,
	0xA5, 0x0E, 0x8D, 0xE3, 0x31, 0x23, 0x69, 0xAA, 0x93, 0x9A, 0x32, 0x17,
	0xFD, 0xFD, 0xF4, 0x54, 0x16, 0x93, 0x2F, 0xF5, 0xD2, 0x87, 0x8B, 0xA2,
	0x18, 0xAF, 0xC8, 0x8B, 0xF9, 0xBA, 0xBA, 0x2B, 0xD7, 0x0D, 0x13, 0x56,
	0x89, 0xFF, 0x1F, 0xDE, 0x3A, 0x2E, 0x54, 0xCA, 0x85, 0x3D, 0xEC, 0x6D,
	0x8A, 0xE9, 0x01, 0x50, 0xD9, 0xB1, 0xFC, 0x9B, 0x64, 0x5D, 0xEA, 0x0F,
	0xF4, 0xA5, 0x77, 0xA9, 0x00, 0x6F, 0x4A, 0xDD, 0x6F, 0x9F, 0xD3, 0x00,
	0x37, 0x34, 0x43, 0x2D, 0xC9, 0xD1, 0xC8, 0x82, 0x67, 0xAA, 0x0C, 0xDC,
	0x54, 0x83, 0xC9, 0x2E, 0xC6, 0x62, 0x7B, 0x10, 0xD6, 0xC7, 0x30, 0xE7,
	0x3F, 0x36, 0x1F, 0x4B, 0xA7, 0x03, 0x86, 0x50, 0x40, 0x9D, 0x6E, 0xC1,
	0xAD, 0x2E, 0x21, 0x69, 0x29, 0x09, 0x7C, 0x91, 0xC9, 0xBE, 0xA9, 0x89,
	0xDC, 0x80, 0x8F, 0x4E, 0x95, 0xEE, 0xAD, 0x37, 0x12, 0xD2, 0x10, 0x82,
	0x7F, 0x22, 0xB9, 0xF5, 0x78, 0xBA, 0x77, 0x0E, 0x36, 0x4A, 0x74, 0x1D,
	0x54, 0x1C, 0xB9, 0x2D, 0xEE, 0x93, 0xA4, 0x51, 0x3F, 0x6E, 0x90, 0x71,
	0xD6, 0x2A, 0x06, 0x5D, 0xCE, 0x39, 0x81, 0xE0, 0x1D, 0xE2, 0x1B, 0x00,
	0xD2, 0xD7, 0x80, 0x45, 0x03, 0x9B, 0xA7, 0x22, 0x86, 0x13, 0xEA, 0x61,
	0x1B, 0xC8, 0x16, 0x2E, 0x63, 0x66, 0x59, 0x85, 0x7C, 0x41, 0x4B, 0xBE,
	0x74, 0xE0, 0x0B, 0xCF, 0x22, 0x32, 0x28, 0xC2, 0x42, 0x8A, 0x5A, 0x2E,
	0x15, 0x88, 0x06, 0x8B, 0x3E, 0xD6, 0x5B, 0xD0, 0x4F, 0x9C, 0x33, 0x21,
	0x4F, 0xD6, 0x8F, 0xE6, 0xAA, 0xBE, 0x95, 0x91, 0x4F, 0x4E, 0x45, 0x5B,
	0x8A, 0x47, 0xFF, 0x39
};

static const unsigned char TA0_RSA_E[] = {
	0x01, 0x00, 0x01
};

static const br_x509_trust_anchor TAs[1] = {
	{
		{ (unsigned char *)TA0_DN, sizeof TA0_DN },
		0,
		{
			BR_KEYTYPE_RSA,
			{ .rsa = {
				(unsigned char *)TA0_RSA_N, sizeof TA0_RSA_N,
				(unsigned char *)TA0_RSA_E, sizeof TA0_RSA_E,
			} }
		}
	}
};
