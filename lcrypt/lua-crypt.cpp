
#include "md5.h"
#include "xxtea.h"
#include "des56.h"
#include "base64.h"
#include "quickzip/quick_zip.h"

#define SMALL_CHUNK 256

static int lrandomkey(lua_State *L)
{
	char tmp[8];
	int i;
	for (i=0;i<8;i++)
    {
		tmp[i] = random() & 0xff;
	}
	lua_pushlstring(L, tmp, 8);
	return 1;
}

static void hash(const char * str, int sz, uint8_t key[8])
{
	uint32_t djb_hash = 5381L;
	uint32_t js_hash = 1315423911L;
	int i;
	for (i=0;i<sz;i++)
    {
		uint8_t c = (uint8_t)str[i];
		djb_hash += (djb_hash << 5) + c;
		js_hash ^= ((js_hash << 5) + c + (js_hash >> 2));
	}

	key[0] = djb_hash & 0xff;
	key[1] = (djb_hash >> 8) & 0xff;
	key[2] = (djb_hash >> 16) & 0xff;
	key[3] = (djb_hash >> 24) & 0xff;

	key[4] = js_hash & 0xff;
	key[5] = (js_hash >> 8) & 0xff;
	key[6] = (js_hash >> 16) & 0xff;
	key[7] = (js_hash >> 24) & 0xff;
}

static int
lhashkey(lua_State *L)
{
	size_t sz = 0;
	const char * key = luaL_checklstring(L, 1, &sz);
	uint8_t realkey[8];
	hash(key,(int)sz,realkey);
	lua_pushlstring(L, (const char *)realkey, 8);
	return 1;
}

static int ltohex(lua_State *L)
{
	static char hex[] = "0123456789abcdef";
	size_t sz = 0;
	const uint8_t * text = (const uint8_t *)luaL_checklstring(L, 1, &sz);
	char tmp[SMALL_CHUNK];
	char *buffer = tmp;
	if (sz > SMALL_CHUNK/2)
    {
		buffer = lua_newuserdata(L, sz * 2);
	}
	int i;
	for (i=0;i<sz;i++)
    {
		buffer[i*2] = hex[text[i] >> 4];
		buffer[i*2+1] = hex[text[i] & 0xf];
	}
	lua_pushlstring(L, buffer, sz * 2);
	return 1;
}

#define HEX(v,c) { char tmp = (char) c; if (tmp >= '0' && tmp <= '9') { v = tmp-'0'; } else { v = tmp - 'a' + 10; } }

static int lfromhex(lua_State *L)
{
	size_t sz = 0;
	const char * text = luaL_checklstring(L, 1, &sz);
	if (sz & 2) {
		return luaL_error(L, "Invalid hex text size %d", (int)sz);
	}
	char tmp[SMALL_CHUNK];
	char *buffer = tmp;
	if (sz > SMALL_CHUNK*2) {
		buffer = lua_newuserdata(L, sz / 2);
	}
	int i;
	for (i=0;i<sz;i+=2) {
		uint8_t hi,low;
		HEX(hi, text[i]);
		HEX(low, text[i+1]);
		if (hi > 16 || low > 16) {
			return luaL_error(L, "Invalid hex text", text);
		}
		buffer[i/2] = hi<<4 | low;
	}
	lua_pushlstring(L, buffer, i/2);
	return 1;
}

static int lxxtea_encode(lua_State* L)
{
    size_t data_len = 0;
    size_t encode_len = 0;
    const char* key = luaL_checkstring(L, 1);
    const char* message = luaL_checklstring(L, 2, &data_len);
    char* encode_out = xxtea_encrypt(message, data_len, key, &encode_out);
    lua_pushlstring(L, encode_out, encode_len);
    free(encode_out);
    return 1;
}

static int lxxtea_decode(lua_State* L)
{
    size_t data_len = 0;
    size_t decode_len = 0;
    const char* key = luaL_checkstring(L, 1);
    const char* message = luaL_checklstring(L, 2, &data_len);
    char* decode_out = xxtea_decrypt(message, data_len, key, &decode_out);
    lua_pushlstring(L, decode_out, decode_len);
    free(decode_out);
    return 1;
}

static int lbase64_encode(lua_State* L)
{
    size_t data_len = 0;
    const char* message = luaL_checklstring(L, 1, &data_len);
    char* encode_out = malloc(BASE64_ENCODE_OUT_SIZE(data_len));
    unsigned int encode_len = base64_encode(message, data_len, encode_out);
    lua_pushlstring(L, encode_out, encode_len);
    free(encode_out);
    return 1;
}

static int lbase64_decode(lua_State* L)
{
    size_t data_len = 0;
    const char* message = luaL_checklstring(L, 1, &data_len);
    unsigned char* decode_out = malloc(BASE64_DECODE_OUT_SIZE(data_len));
    unsigned int decode_len = base64_decode((char*)message, data_len, decode_out);
    lua_pushlstring(L, decode_out, decode_len);
    free(decode_out);
    return 1;
}

static int lquick_zip(lua_State* L)
{
    QuickZip zip;
    size_t data_len = 0;
    const char* message = luaL_checklstring(L, 1, &data_len);
    ByteContainer zip_bytes = zip.Zip((char*)message, data_len);
    lua_pushlstring(L, zip_bytes.buffer, zip_bytes.size);
    return 1;
}

static int lquick_unzip(lua_State* L)
{
    QuickZip zip;
    size_t data_len = 0;
    const char* message = luaL_checklstring(L, 1, &data_len);
    ByteContainer unzip_bytes = zip.Unzip((char*)message, data_len);
    lua_pushlstring(L, unzip_bytes.buffer, unzip_bytes.size);
    return 1;
}

static int lmd5(lua_State* L)
{
    size_t data_len = 0;
    const char* message = luaL_checklstring(L, 1, &data_len);
    char output[HASHSIZE];
    md5(message, data_len, output);
    lua_pushstring(L, output);
    return 1;
}

static int des56_decrypt( lua_State *L )
{
    char* decypheredText;
    keysched KS;
    int rel_index, abs_index;
    size_t cypherlen;
    const char *cypheredText = luaL_checklstring(L, 1, &cypherlen);
    const char *key = luaL_optstring(L, 2, NULL);
    int padinfo;

    padinfo = cypheredText[cypherlen-1];
    cypherlen--;

    /* Aloca array */
    decypheredText = (char *) malloc((cypherlen+1) * sizeof(char));
    /* Inicia decifragem */
    if (key && strlen(key) >= 8)
    {
        char k[8];
        int i;

        for (i=0; i<8; i++)
            k[i] = (unsigned char)key[i];
        fsetkey(k, &KS);
    }
    else
    {
        lua_error(L, "Error decrypting file. Invalid key.");
    }
    rel_index = 0;
    abs_index = 0;
    while (abs_index < (int) cypherlen)
    {
        decypheredText[abs_index] = cypheredText[abs_index];
        abs_index++;
        rel_index++;
        if( rel_index == 8 )
        {
            rel_index = 0;
            fencrypt(&(decypheredText[abs_index - 8]), 1, &KS);
        }
    }
    decypheredText[abs_index] = 0;
    lua_pushlstring(L, decypheredText, (abs_index-padinfo));
    free(decypheredText);
    return 1;
}

static int des56_crypt( lua_State *L )
{
    char *cypheredText;
    keysched KS;
    int rel_index, pad, abs_index;
    size_t plainlen;
    const char *plainText = luaL_checklstring( L, 1, &plainlen );
    const char *key = luaL_optstring( L, 2, NULL );

    cypheredText = (char *) malloc( (plainlen+8) * sizeof(char));
    if (key && strlen(key) >= 8)
    {
        char k[8];
        int i;
        for (i=0; i<8; i++)
            k[i] = (unsigned char)key[i];
        fsetkey(k, &KS);
    }
    else
    {
        lua_error(L, "Error encrypting file. Invalid key.");
    }

    rel_index = 0;
    abs_index = 0;
    while (abs_index < (int) plainlen) 
    {
        cypheredText[abs_index] = plainText[abs_index];
        abs_index++;
        rel_index++;
        if( rel_index == 8 ) 
        {
            rel_index = 0;
            fencrypt(&(cypheredText[abs_index - 8]), 0, &KS);
        }
    }

    pad = 0;
    if(rel_index != 0) 
    { /* Pads remaining bytes with zeroes */
        while(rel_index < 8)
        {
            pad++;
            cypheredText[abs_index++] = 0;
            rel_index++;
        }
        fencrypt(&(cypheredText[abs_index - 8]), 0, &KS);
    }
    cypheredText[abs_index] = pad;
    lua_pushlstring( L, cypheredText, abs_index+1 );
    free( cypheredText );
    return 1;
}

#ifdef _MSC_VER
#define LUACRYPT_API _declspec(dllexport)
#else
#define LUACRYPT_API
#endif

LUACRYPT_API int luaopen_crypt(lua_State* L)
{
    luaL_checkversion(L);

    luaL_Reg l[] = {
        { "md5", lmd5 },
        { "hashkey", lhashkey },
		{ "randomkey", lrandomkey },
        { "hex_encode", ltohex },
		{ "hex_decode", lfromhex },
        { "des_encode", des56_crypt },
        { "des_encode", des56_decrypt },
        { "quick_zip", lquick_zip },
        { "quick_unzip", lquick_unzip },
        { "b64_encode", lbase64_encode },
        { "b64_decode", lbase64_decode },
        { "xxtea_encode", lxxtea_encode },
        { "xxtea_decode", lxxtea_decode },
        { NULL, NULL },
    };

    luaL_newlib(L, l);

    return 1;
}