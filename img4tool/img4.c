//
//  img4.c
//  img4tool
//
//  Created by tihmstar on 15.06.16.
//  Copyright © 2016 tihmstar. All rights reserved.
//

#include "img4.h"
#include "all_img4tool.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#ifdef __APPLE__
#   include <CommonCrypto/CommonDigest.h>
#   define SHA1(d, n, md) CC_SHA1(d, n, md)
#   define SHA_DIGEST_LENGTH CC_SHA1_DIGEST_LENGTH
#else
#   include <openssl/sha.h>
#endif // __APPLE__

#define safeFree(buf) if (buf) free(buf), buf = NULL

t_asn1ElemLen asn1Len(const char buf[4]){
    t_asn1Length *sTmp = (t_asn1Length *)buf;
    size_t outSize = 0;
    int sizeBytes_ = 0;
    
    unsigned char *sbuf = (unsigned char *)buf;
    
    if (!sTmp->isLong) outSize = sTmp->len;
    else{
        sizeBytes_ = sTmp->len;
        for (int i=0; i<sizeBytes_; i++) {
            outSize *= 0x100;
            outSize += sbuf[1+i];
        }
    }
    
    t_asn1ElemLen ret;
    ret.dataLen = outSize;
    ret.sizeBytes = sizeBytes_+1;
    return ret;
}

char *ans1GetString(char *buf, char **outString, size_t *strlen){
    
    t_asn1Tag *tag = (t_asn1Tag *)buf;
    
    if (!(tag->tagNumber | kASN1TagIA5String)) {
        error("not a string\n");
        return 0;
    }
    
    t_asn1ElemLen len = asn1Len(++buf);
    *strlen = len.dataLen;
    buf+=len.sizeBytes;
    if (outString) *outString = buf;
    
    return buf+*strlen;
}

int asn1ElementAtIndexWithCounter(const char *buf, int index, t_asn1Tag **tagret){
    int ret = 0;
    
    if (!((t_asn1Tag *)buf)->isConstructed) return 0;
    t_asn1ElemLen len = asn1Len(++buf);
    
    buf +=len.sizeBytes;
    if (index == 0 && tagret){
        *tagret = (t_asn1Tag *)buf;
        return 1;
    }
    if (*buf == kASN1TagPrivate){
        size_t sb;
        asn1GetPrivateTagnum((t_asn1Tag*)buf,&sb);
        buf+=sb;
        len.dataLen-=sb;
    }else buf++;
    
    while (len.dataLen) {
        t_asn1ElemLen sublen = asn1Len(buf);
        size_t toadd =sublen.dataLen + sublen.sizeBytes;
        len.dataLen -=toadd;
        buf +=toadd;
        ret ++;
        if (len.dataLen <=1) break;
        if (--index == 0 && tagret){
            *tagret = (t_asn1Tag*)buf;
            return ret;
        }
        if (*buf == kASN1TagPrivate){
            size_t sb;
            asn1GetPrivateTagnum((t_asn1Tag*)buf,&sb);
            buf+=sb;
            len.dataLen-=sb;
            
        } else buf++,len.dataLen--;
        
    }
    return ret;
}

int asn1ElementsInObject(const char *buf){
    return asn1ElementAtIndexWithCounter(buf, -1, NULL);
}

t_asn1Tag *asn1ElementAtIndex(const char *buf, int index){
    t_asn1Tag *ret;
    asn1ElementAtIndexWithCounter(buf, index, &ret);
    return ret;
}

int getSequenceName(const char *buf,char**name, size_t *nameLen){
#define reterror(a ...){error(a); err = -1; goto error;}
    int err = 0;
    if (((t_asn1Tag*)buf)->tagNumber != kASN1TagSEQUENCE) reterror("not a SEQUENCE");
    int elems = asn1ElementsInObject(buf);
    if (!elems) reterror("no elements in SEQUENCE\n");
    size_t len;
    ans1GetString((char*)asn1ElementAtIndex(buf,0),name,&len);
    if (nameLen) *nameLen = len;
error:
    return err;
#undef reterror
}

size_t asn1GetPrivateTagnum(t_asn1Tag *tag, size_t *sizebytes){
    if (*(unsigned char*)tag != 0xff) {
        error("not a private TAG 0x%02x\n",*(unsigned int*)tag);
        return 0;
    }
    size_t sb = 1;
    t_asn1ElemLen taglen = asn1Len((char*)++tag);
    taglen.sizeBytes-=1;
    if (taglen.sizeBytes != 4){
        /* 
         WARNING: seems like apple's private tag is always 4 bytes long
         i first assumed 0x84 can be parsed as long size with 4 bytes, 
         but 0x86 also seems to be 4 bytes size even if one would assume it means 6 bytes size.
         This opens the question what the 4 or 6 nibble means.
        */
        taglen.sizeBytes = 4;
    }
    size_t tagname =0;
    do {
        tagname *=0x100;
        tagname>>=1;
        tagname += ((t_asn1PrivateTag*)tag)->num;
        sb++;
    } while (((t_asn1PrivateTag*)tag++)->more);
    if (sizebytes) *sizebytes = sb;
    return tagname;
}

uint64_t ans1GetNumberFromTag(t_asn1Tag *tag){
    if (tag->tagNumber != kASN1TagINTEGER) return (error("not an INTEGER\n"),0);
    uint64_t ret = 0;
    t_asn1ElemLen len = asn1Len((char*)++tag);
    unsigned char *data = (unsigned char*)tag+len.sizeBytes;
    while (len.dataLen--) {
        ret *= 0x100;
        ret+= *data++;
    }
    
    return ret;
}

void printStringWithKey(char*key, t_asn1Tag *string){
    char *str = 0;
    size_t strlen;
    ans1GetString((char*)string,&str,&strlen);
    printf("%s",key);
    putStr(str, strlen);
    putchar('\n');
}

void printPrivtag(size_t privTag){
    char *ptag = (char*)&privTag;
    int len = 0;
    while (*ptag) ptag++,len++;
    while (len--) putchar(*--ptag);
}

void printHexString(t_asn1Tag *str){
    if (str->tagNumber != kASN1TagOCTET){
        error("not an OCTET string\n");
        return;
    }
    
    t_asn1ElemLen len = asn1Len((char*)str+1);
    
    unsigned char *string = (unsigned char*)str + len.sizeBytes +1;
    
    while (len.dataLen--) printf("%02x",*string++);
}

void printI5AString(t_asn1Tag *str){
    if (str->tagNumber != kASN1TagIA5String){
        error("not an I5A string\n");
        return;
    }
    
    t_asn1ElemLen len = asn1Len((char*)++str);
    putStr(((char*)str)+len.sizeBytes, len.dataLen);
}

void printKBAGOctet(char *octet){
#define reterror(a ...){error(a);goto error;}
    if (((t_asn1Tag*)octet)->tagNumber != kASN1TagOCTET) reterror("not an OCTET\n");
    
    t_asn1ElemLen octetlen = asn1Len(++octet);
    octet +=octetlen.sizeBytes;
    //main seq
    int subseqs = asn1ElementsInObject(octet);
    for (int i=0; i<subseqs; i++) {
        char *s = (char*)asn1ElementAtIndex(octet, i);
        int elems = asn1ElementsInObject(s);
        
        if (elems--){
            //integer (currently unknown?)
            t_asn1Tag *num = asn1ElementAtIndex(s, 0);
            if (num->tagNumber != kASN1TagINTEGER) warning("skipping unexpected tag\n");
            else{
                char n = *(char*)(num+2);
                printf("num: %d\n",n);
            }
        }
        if (elems--)printHexString(asn1ElementAtIndex(s, 1)),putchar('\n');
        if (elems--)printHexString(asn1ElementAtIndex(s, 2)),putchar('\n');
    }
    
error:
    return;
#undef reterror
}

void printNumber(t_asn1Tag *tag){
    if (tag->tagNumber != kASN1TagINTEGER) {
        error("tag not an INTEGER\n");
        return;
    }
    t_asn1ElemLen len = asn1Len((char*)++tag);
    uint32_t num = 0;
    while (len.sizeBytes--) {
        num *=0x100;
        num += *(unsigned char*)++tag;
    }
    printf("%u",num);
}

void printIM4P(char *buf){
#define reterror(a ...){error(a);goto error;}
    
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("IM4P", magic, l)) reterror("unexpected \"%.*s\", expected \"IM4P\"\n",(int)l,magic);
    
    int elems = asn1ElementsInObject(buf);
    if (--elems>0) printStringWithKey("type: ",asn1ElementAtIndex(buf, 1));
    if (--elems>0) printStringWithKey("desc: ",asn1ElementAtIndex(buf, 2));
    if (--elems>0) {
        //data
        t_asn1Tag *data =asn1ElementAtIndex(buf, 3);
        if (data->tagNumber != kASN1TagOCTET) warning("skipped an unexpected tag where OCTETSTING was expected\n");
        else printf("size: 0x%08zx\n",asn1Len((char*)data+1).dataLen);
    }
    if (--elems>0) {
        //kbag values
        printf("\nKBAG\n");
        printKBAGOctet((char*)asn1ElementAtIndex(buf, 4));
    }else{
        printf("\nIM4P does not contain KBAG values\n");
    }
    
error:
    return;
#undef reterror
}

int extractFileFromIM4P(char *buf, const char *dstFilename){
    int elems = asn1ElementsInObject(buf);
    if (elems < 4){
        error("not enough elements in SEQUENCE %d\n",elems);
        return -2;
    }
    
    t_asn1Tag *dataTag = asn1ElementAtIndex(buf, 3)+1;
    t_asn1ElemLen dlen = asn1Len((char*)dataTag);
    char *data = (char*)dataTag+dlen.sizeBytes;
    
    FILE *f = fopen(dstFilename, "wb");
    if (!f) {
        error("can't open file %s\n",dstFilename);
        return -1;
    }
    fwrite(data, dlen.dataLen, 1, f);
    fclose(f);
    
    return 0;
}

int sequenceHasName(const char *buf, char *name){
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    return strncmp(name, magic, l) == 0;
}

char *getElementFromIMG4(char *buf, char* element){
#define reterror(a ...) return (error(a),NULL)
    if (!sequenceHasName(buf, "IMG4")) reterror("not img4 sequcence\n");
    
    int elems = asn1ElementsInObject(buf);
    for (int i=0; i<elems; i++) {
        
        t_asn1Tag *elemen = asn1ElementAtIndex(buf, i);
        
        if (elemen->tagNumber != kASN1TagSEQUENCE && elemen->tagClass == kASN1TagClassContextSpecific) {
            //assuming we found a "subcontainer"
            elemen += asn1Len((char*)elemen+1).sizeBytes+1;
        }
        
        if (elemen->tagNumber == kASN1TagSEQUENCE && sequenceHasName((char*)elemen, element)) {
            return (char*)elemen;
        }
    }
    reterror("element %s not found in IMG4\n",element);
#undef reterror
}

int extractElementFromIMG4(char *buf, char* element, const char *dstFilename){
#define reterror(a ...) return (error(a),-1)
    
    char *elemen = getElementFromIMG4(buf, element);
    if (!elemen) return -1;
    FILE *f = fopen(dstFilename, "wb");
    if (!f) {
        error("can't open file %s\n",dstFilename);
        return -1;
    }
    
    t_asn1ElemLen len = asn1Len((char*)elemen+1);
    size_t flen = len.dataLen + len.sizeBytes +1;
    fwrite(elemen, flen, 1, f);
    fclose(f);
    
    return 0;
#undef reterror
}

int asn1MakeSize(char *sizeBytesDst, size_t size){
    int off = 0;
    if (size >= 0x1000000) {
        // 1+4 bytes length
        sizeBytesDst[off++] = 0x84;
        sizeBytesDst[off++] = (size >> 24) & 0xFF;
        sizeBytesDst[off++] = (size >> 16) & 0xFF;
        sizeBytesDst[off++] = (size >> 8) & 0xFF;
        sizeBytesDst[off++] = size & 0xFF;
    } else if (size >= 0x10000) {
        // 1+3 bytes length
        sizeBytesDst[off++] = 0x83;
        sizeBytesDst[off++] = (size >> 16) & 0xFF;
        sizeBytesDst[off++] = (size >> 8) & 0xFF;
        sizeBytesDst[off++] = size & 0xFF;
    } else if (size >= 0x100) {
        // 1+2 bytes length
        sizeBytesDst[off++] = 0x82;
        sizeBytesDst[off++] = (size >> 8) & 0xFF;
        sizeBytesDst[off++] = (size & 0xFF);
    } else if (size >= 0x80) {
        // 1+1 byte length
        sizeBytesDst[off++] = 0x81;
        sizeBytesDst[off++] = (size & 0xFF);
    } else {
        // 1 byte length
        sizeBytesDst[off++] = size & 0xFF;
    }
    return off;
}

char *asn1PrepandTag(char *buf, t_asn1Tag tag){
    t_asn1ElemLen len = asn1Len(buf+1);
    
    //alloc mem for oldTag+oldSizebytes+oldData  + newTag + newTagSizebytesMax
    char *ret = malloc(len.sizeBytes + len.dataLen +1 +1+4);
    ret[0] = *(char*)&tag;
    int nSizeBytes = asn1MakeSize(ret+1, len.sizeBytes + len.dataLen +1);
    memcpy(ret + nSizeBytes+1, buf, len.sizeBytes + len.dataLen +1);
    return ret;
}

char *asn1AppendToTag(char *buf, char *toappend){
    t_asn1ElemLen buflen = asn1Len(buf+1);
    t_asn1ElemLen apndLen = asn1Len(toappend+1);
    
    //alloc memory for bufdata + buftag + apndData + apndSizebytes + apndTag + maxSizeBytesForBuf
    size_t containerLen;
    char *ret = malloc(1 +(containerLen = buflen.dataLen +apndLen.sizeBytes + apndLen.dataLen +1) +4);
    
    ret[0] = buf[0];
    int nSizeBytes = asn1MakeSize(ret+1, containerLen);
    //copy old data
    memcpy(ret + nSizeBytes+1, buf+1+buflen.sizeBytes, buflen.dataLen);
    
    
    memcpy(ret +nSizeBytes+1+ buflen.dataLen, toappend, apndLen.sizeBytes +apndLen.dataLen +1);
    free(buf);
    
    return ret;
}

char *makeIM4RWithNonce(char *nonce){
    char template[] = {0xA1, 0x23, 0x30, 0x21, 0x16, 0x04, 0x49, 0x4D,
                       0x34, 0x52, 0x31, 0x19, 0xFF, 0x84, 0x92, 0xB9,
                       0x86, 0x4E, 0x12, 0x30, 0x10, 0x16, 0x04, 0x42,
                       0x4E, 0x43, 0x4E, 0x04, 0x08};
    char *ret = malloc(sizeof(template)+8);
    strncpy(ret, template,sizeof(template));
    strncpy(ret+sizeof(template), nonce, 8);
    return ret;
}

char *makeIMG4(char *im4p, char *im4m, char *im4r, size_t *size){
    t_asn1Tag elem0;
    elem0.tagNumber = 0;
    elem0.tagClass = kASN1TagClassContextSpecific;
    elem0.isConstructed = 1;
    if (im4m) im4m = asn1PrepandTag(im4m, elem0);
    
    char *sequence = malloc(2);
    sequence[0] = 0x30;
    sequence[1] = 0x00;
    
    char iA5String_IMG4[] = {0x16, 0x04, 0x49, 0x4D, 0x47, 0x34};
    
    sequence = asn1AppendToTag(sequence, iA5String_IMG4);
    if (im4p) sequence = asn1AppendToTag(sequence, im4p);
    if (im4m) sequence = asn1AppendToTag(sequence, im4m);
    if (im4r) {
        char *noncebuf = makeIM4RWithNonce(im4r);
        sequence = asn1AppendToTag(sequence, noncebuf);
        free(noncebuf);
    }
    
    if (size){
        t_asn1ElemLen retlen = asn1Len(sequence+1);
        *size = 1+ retlen.dataLen + retlen.sizeBytes;
    }
    free(im4m); //only freeing local copy, not actually freeing outside im4m buffer
    
    return sequence;
}

int replaceNameInIM4P(char *buf, const char *newName){
    
    if (asn1ElementsInObject(buf)<2){
        error("not enough objects in sequence\n");
        return -1;
    }
    
    t_asn1Tag *nameTag = asn1ElementAtIndex(buf, 1);
    
    if (nameTag->tagNumber != kASN1TagIA5String){
        error("nameTag is not IA5String\n");
        return -2;
    }
    t_asn1ElemLen len;
    if ((len = asn1Len((char*)nameTag+1)).dataLen !=4){
        error("nameTag has not a length of 4 Bytes, actual len=%ld\n",len.dataLen);
        return -2;
    }
    
    memmove(nameTag + 1 + len.sizeBytes, newName, 4);
    
    return 0;
}


char *getValueForTagInSet(char *set, uint32_t tag){
#define reterror(a) return (error(a),NULL)
    
    if (((t_asn1Tag*)set)->tagNumber != kASN1TagSET) reterror("not a SET\n");
    t_asn1ElemLen setlen = asn1Len(++set);
    
    for (char *setelems = set+setlen.sizeBytes; setelems<set+setlen.dataLen;) {
        
        if (*(unsigned char*)setelems == 0xff) {
            //priv tag
            size_t sb;
            size_t ptag = asn1GetPrivateTagnum((t_asn1Tag*)setelems,&sb);
            setelems += sb;
            t_asn1ElemLen len = asn1Len(setelems);
            setelems += len.sizeBytes;
            if (tag == ptag) return setelems;
            setelems +=len.dataLen;
        }else{
            //normal tag
            t_asn1ElemLen len = asn1Len(setelems);
            setelems += len.sizeBytes + 1;
            if (((t_asn1Tag*)setelems)->tagNumber == tag) return setelems;
            setelems += len.dataLen;
        }
    }
    return 0;
#undef reterror
}

void printElemsInIMG4(char *buf, bool printAll, bool im4pOnly){
#define reterror(a...) {error(a); goto error;}
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("IMG4", magic, l)) reterror("unexpected \"%.*s\", expected \"IMG4\"\n",(int)l,magic);
    printf("IMG4:\n");
    int elems = asn1ElementsInObject(buf);
    
    for (int i=1; i<elems; i++) {
        char *tag = (char*)asn1ElementAtIndex(buf, i);
        
        if (((t_asn1Tag*)tag)->tagClass == kASN1TagClassContextSpecific) {
            tag += asn1Len((char*)tag+1).sizeBytes +1;
        }
        
        char *magic = 0;
        size_t l;
        getSequenceName((char*)tag, &magic, &l);
        
        putStr(magic, l);printf(": ---------\n");
        
        if (!im4pOnly && strncmp("IM4R", magic, l) == 0) printIM4R(tag);
        if (!im4pOnly && strncmp("IM4M", magic, l) == 0) printIM4M(tag,printAll);
        if (strncmp("IM4P", magic, l) == 0) printIM4P(tag);
        putchar('\n');
    }
    
error:
    return;
#undef reterror
}


void printIM4R(char *buf){
#define reterror(a ...){error(a);goto error;}
    
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("IM4R", magic, l)) reterror("unexpected \"%.*s\", expected \"IM4R\"\n",(int)l,magic);
    
    int elems = asn1ElementsInObject(buf);
    if (elems<2) reterror("expecting at least 2 elements\n");
    
    t_asn1Tag *set = asn1ElementAtIndex(buf, 1);
    if (set->tagNumber != kASN1TagSET) reterror("expecting SET type\n");
    
    set += asn1Len((char*)set+1).sizeBytes+1;
    
    if (set->tagClass != kASN1TagClassPrivate) reterror("expecting PRIVATE type\n");
    
    printPrivtag(asn1GetPrivateTagnum(set++,0));
    printf("\n");
    
    set += asn1Len((char*)set).sizeBytes+1;
    elems = asn1ElementsInObject((char*)set);
    if (elems<2) reterror("expecting at least 2 elements\n");
    
    printI5AString(asn1ElementAtIndex((char*)set, 0));
    printf(": ");
    printHexString(asn1ElementAtIndex((char*)set, 1));
    putchar('\n');
    
error:
    return;
#undef reterror
}

char *getIM4PFromIMG4(char *buf){
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("IMG4", magic, l)) return error("unexpected \"%.*s\", expected \"IMG4\"\n",(int)l,magic),NULL;
    if (asn1ElementsInObject(buf)<2) return error("not enough elements in SEQUENCE"),NULL;
    char *ret = (char*)asn1ElementAtIndex(buf, 1);
    getSequenceName(ret, &magic, &l);
    return (strncmp("IM4P", magic, 4) == 0) ? ret : (error("unexpected \"%.*s\", expected \"IM4P\"\n",(int)l,magic),NULL);
}

char *getIM4MFromIMG4(char *buf){
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("IMG4", magic, l)) return error("unexpected \"%.*s\", expected \"IMG4\"\n",(int)l,magic),NULL;
    if (asn1ElementsInObject(buf)<3) return error("not enough elements in SEQUENCE"),NULL;
    char *ret = (char*)asn1ElementAtIndex(buf, 2);
    if (((t_asn1Tag*)ret)->tagClass != kASN1TagClassContextSpecific) return error("unexpected Tag 0x%02x, expected SET\n",*(unsigned char*)ret),NULL;
    ret += asn1Len(ret+1).sizeBytes + 1;
    getSequenceName(ret, &magic, &l);
    return (strncmp("IM4M", magic, 4) == 0) ? ret : NULL;
}

void printIM4M(char *buf, bool printAll){
#define reterror(a ...){error(a);goto error;}
    
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("IM4M", magic, l)) reterror("unexpected \"%.*s\", expected \"IM4M\"\n",(int)l,magic);
    
    int elems = asn1ElementsInObject(buf);
    if (elems<2) reterror("expecting at least 2 elements\n");
    
    if (--elems>0) {
        printf("Version: ");
        printNumber(asn1ElementAtIndex(buf, 1));
        putchar('\n');
    }
    if (--elems>0) {
        t_asn1Tag *manbset = asn1ElementAtIndex(buf, 2);
        if (manbset->tagNumber != kASN1TagSET) reterror("expecting SET\n");
        
        t_asn1Tag *privtag = manbset + asn1Len((char*)manbset+1).sizeBytes+1;
        size_t sb;
        printPrivtag(asn1GetPrivateTagnum(privtag++,&sb));
        printf("\n");
        char *manbseq = (char*)privtag+sb;
        manbseq+= asn1Len(manbseq).sizeBytes+1;
        printMANB(manbseq, printAll);
        if (!printAll) return;
    }
//    if (--elems>0){
//        printf("signed hash: ");
//        printHexString(asn1ElementAtIndex(buf, 3));
//        putchar('\n');
//    }
//    if (--elems>0){
//#warning TODO print apple certificate?
//    }
    
    
error:
    return;
#undef reterror
}

void asn1PrintValue(t_asn1Tag *tag){
    if (tag->tagNumber == kASN1TagIA5String){
        printI5AString(tag);
    }else if (tag->tagNumber == kASN1TagOCTET){
        printHexString(tag);
    }else if (tag->tagNumber == kASN1TagINTEGER){
        t_asn1ElemLen len = asn1Len((char*)tag+1);
        unsigned char *num = (unsigned char*)tag+1 + len.sizeBytes;
        uint64_t pnum = 0;
        while (len.dataLen--) {
            pnum *=0x100;
            pnum += *num++;
            
            //ungly workaround for WIN32
            if (sizeof(uint64_t) != 8) printf("%02x",num[-1]);
            
        }
        if (sizeof(uint64_t) == 8) printf("%llu",pnum);
        else printf(" (hex)");
    }else if (tag->tagNumber == kASN1TagBOOLEAN){
        printf("%s",(*(char*)tag+2 == 0) ? "false" : "true");
    }else{
        error("can't print unknown tag %02x\n",*(unsigned char*)tag);
    }
}

void asn1PrintRecKeyVal(char *buf){
    
    if (((t_asn1Tag*)buf)->tagNumber == kASN1TagSEQUENCE) {
        int i;
        if ((i = asn1ElementsInObject(buf)) != 2){
            error("expecting 2 elements found %d\n",i);
            return;
        }
        printI5AString(asn1ElementAtIndex(buf, 0));
        printf(": ");
        asn1PrintRecKeyVal((char*)asn1ElementAtIndex(buf, 1));
        printf("\n");
        return;
    }else if (((t_asn1Tag*)buf)->tagNumber != kASN1TagSET){
        asn1PrintValue((t_asn1Tag *)buf);
        return;
    }
    
    
    //must be a SET
    printf("------------------------------\n");
    for (int i = 0; i<asn1ElementsInObject(buf); i++) {
        char *elem = (char*)asn1ElementAtIndex(buf, i);
        size_t sb;
        printPrivtag(asn1GetPrivateTagnum((t_asn1Tag*)elem,&sb));
        printf(": ");
        elem+=sb;
        elem += asn1Len(elem+1).sizeBytes;
        asn1PrintRecKeyVal(elem);
    }
    
}

void printMANB(char *buf, bool printAll){
#define reterror(a ...){error(a);goto error;}
    
    char *magic;
    size_t l;
    getSequenceName(buf, &magic, &l);
    if (strncmp("MANB", magic, l)) reterror("unexpected \"%.*s\", expected \"MANB\"\n",(int)l,magic);
    
    int manbElemsCnt = asn1ElementsInObject(buf);
    if (manbElemsCnt<2) reterror("not enough elements in MANB\n");
    char *manbSeq = (char*)asn1ElementAtIndex(buf, 1);
    
    for (int i=0; i<asn1ElementsInObject(manbSeq); i++) {
        t_asn1Tag *manbElem = asn1ElementAtIndex(manbSeq, i);
        size_t privTag = 0;
        if (*(char*)manbElem == kASN1TagPrivate) {
            size_t sb;
            printPrivtag(privTag = asn1GetPrivateTagnum(manbElem,&sb));
            printf(": ");
            manbElem+=sb;
        }else manbElem++;
        
        manbElem += asn1Len((char*)manbElem).sizeBytes;
        
        asn1PrintRecKeyVal((char*)manbElem);
        if (!printAll && strncmp((char*)&privTag, "PNAM", 4) == 0){
            break;
        }
    }
    
    
error:
    return;
#undef reterror
}


char *getSHA1ofSqeuence(char * buf){
    if (((t_asn1Tag*)buf)->tagNumber != kASN1TagSEQUENCE){
        error("tag not seuqnece");
        return 0;
    }
    t_asn1ElemLen bLen = asn1Len(buf+1);
    size_t buflen = 1 + bLen.dataLen + bLen.sizeBytes;
    char *ret = malloc(SHA_DIGEST_LENGTH);
    
    SHA1((unsigned char*)buf, buflen, (unsigned char *)ret);
    
    return 0;
}

int hasBuildidentityElementWithHash(plist_t identity, char *hash, uint64_t hashSize){
#define reterror(a ...){rt=0;error(a);goto error;}
#define skipelem(e) if (strcmp(key, e) == 0) {warning("skipping element=%s\n",key);goto skip;}
    int rt = 0;
    plist_dict_iter dictIterator = NULL;
    
    plist_t manifest = plist_dict_get_item(identity, "Manifest");
    if (!manifest)
        reterror("can't find Manifest\n");
    
    plist_t node = NULL;
    char *key = NULL;
    plist_dict_new_iter(manifest, &dictIterator);
    plist_dict_next_item(manifest, dictIterator, &key, &node);
    do {
        skipelem("BasebandFirmware")
        skipelem("ftap")
        skipelem("ftsp")
        skipelem("rfta")
        skipelem("rfts")
        
        plist_t digest = plist_dict_get_item(node, "Digest");
        if (!digest || plist_get_node_type(digest) != PLIST_DATA)
            reterror("can't find digest for key=%s\n",key);
        
        char *dgstData = NULL;
        uint64_t len = 0;
        plist_get_data_val(digest, &dgstData, &len);
        if (!dgstData)
            reterror("can't get dgstData for key=%s.\n",key);
        
        if (len == hashSize && memcmp(dgstData, hash, len) == 0)
            rt = 1;
        
        free(dgstData);
    skip:
        plist_dict_next_item(manifest, dictIterator, &key, &node);
    } while (!rt && node);
error:
    free(dictIterator),dictIterator = NULL;
    return rt;
#undef reterror
}

plist_t findAnyBuildidentityForFilehash(plist_t identities, char *hash, uint64_t hashSize){
#define skipelem(e) if (strcmp(key, e) == 0) {warning("skipping element=%s\n",key);goto skip;}
#define reterror(a ...){rt=NULL;error(a);goto error;}
    plist_t rt = NULL;
    plist_dict_iter dictIterator = NULL;
    
    for (int i=0; !rt && i<plist_array_get_size(identities); i++) {
        plist_t idi = plist_array_get_item(identities, i);
        
        plist_t manifest = plist_dict_get_item(idi, "Manifest");
        if (!manifest)
            reterror("can't find Manifest. i=%d\n",i);
        
        plist_t node = NULL;
        char *key = NULL;
        plist_dict_new_iter(manifest, &dictIterator);
        plist_dict_next_item(manifest, dictIterator, &key, &node);
        do {
            skipelem("BasebandFirmware")
            skipelem("ftap")
            skipelem("ftsp")
            skipelem("rfta")
            skipelem("rfts")
            
            plist_t digest = plist_dict_get_item(node, "Digest");
            if (!digest || plist_get_node_type(digest) != PLIST_DATA)
                reterror("can't find digest for key=%s. i=%d\n",key,i);
            
            char *dgstData = NULL;
            uint64_t len = 0;
            plist_get_data_val(digest, &dgstData, &len);
            if (!dgstData)
                reterror("can't get dgstData for key=%s. i=%d\n",key,i);
            
            if (len == hashSize && memcmp(dgstData, hash, len) == 0)
                rt = idi;
            
            free(dgstData);
        skip:
            plist_dict_next_item(manifest, dictIterator, &key, &node);
        } while (!rt && node);
        
        free(dictIterator),dictIterator = NULL;
    }
    
error:
    if (dictIterator) free(dictIterator);
    return rt;
#undef reterror
#undef skipelem
}

plist_t getBuildIdentityForIM4M(const char *buf, const plist_t buildmanifest){
#define reterror(a ...){rt=NULL;error(a);goto error;}
#define skipelem(e) if (strncmp(elemNameStr, e, 4) == 0) {warning("skipping element=%s\n",e);continue;}

    plist_t manifest = plist_copy(buildmanifest);
    plist_t rt = NULL;
    
    if (!sequenceHasName(buf, "IM4M"))
        reterror("can't find IM4M tag\n");
    
    char *im4mset = (char *)asn1ElementAtIndex(buf, 2);
    if (!im4mset)
        reterror("can't find im4mset\n");
    char *manbSeq = getValueForTagInSet(im4mset, *(uint32_t*)"BNAM");
    if (!manbSeq)
        reterror("can't find manbSeq\n");
    
    char *manbSet = (char*)asn1ElementAtIndex(manbSeq, 1);
    if (!manbSet)
        reterror("can't find manbSet\n");
    
    plist_t identities = plist_dict_get_item(manifest, "BuildIdentities");
    if (!identities)
        reterror("can't find BuildIdentities\n");
    
    
    for (int i=0; i<asn1ElementsInObject(manbSet); i++) {
        t_asn1Tag *curr = asn1ElementAtIndex(manbSet, i);
        
        size_t sb;
        if (asn1GetPrivateTagnum(curr, &sb) == *(uint32_t*)"PNAM")
            continue;
        char *cSeq = (char*)curr+sb;
        cSeq += asn1Len(cSeq).sizeBytes;
        
        t_asn1Tag *elemName = asn1ElementAtIndex(cSeq, 0);
        t_asn1ElemLen elemNameLen = asn1Len((char*)elemName+1);
        char *elemNameStr = (char*)elemName + elemNameLen.sizeBytes+1;
        
        skipelem("ftsp");
        skipelem("ftap");
        skipelem("rfta");
        skipelem("rfts");
        
        
        char *elemSet = (char*)asn1ElementAtIndex(cSeq, 1);
        if (!elemSet)
            reterror("can't find elemSet. i=%d\n",i);
        
        char *dgstSeq = getValueForTagInSet(elemSet, *(uint32_t*)"TSGD");
        if (!dgstSeq)
            reterror("can't find dgstSeq. i=%d\n",i);
        
        
        t_asn1Tag *dgst = asn1ElementAtIndex(dgstSeq, 1);
        if (!dgst || dgst->tagNumber != kASN1TagOCTET)
            reterror("can't find DGST. i=%d\n",i);
        
        t_asn1ElemLen lenDGST = asn1Len((char*)dgst+1);
        char *dgstData = (char*)dgst+lenDGST.sizeBytes+1;
        
        
        if (rt){
            if (!hasBuildidentityElementWithHash(rt, dgstData, lenDGST.dataLen)){
                //remove identity we are not looking for and start comparing all hashes again
                plist_array_remove_item(identities, plist_array_get_item_index(rt));
                i=-1;
                rt = NULL;
            }
        }else{
            if (!(rt = findAnyBuildidentityForFilehash(identities, dgstData, lenDGST.dataLen)))
                reterror("can't find any identity which matches all hashes inside IM4M\n");
        }
    }
    
    plist_t finfo = plist_dict_get_item(rt, "Info");
    plist_t fdevclass = plist_dict_get_item(finfo, "DeviceClass");
    plist_t fresbeh = plist_dict_get_item(finfo, "RestoreBehavior");
    
    if (!finfo || !fdevclass || !fresbeh)
        reterror("found buildidentiy, but can't read information\n");
    
    plist_t origIdentities = plist_dict_get_item(buildmanifest, "BuildIdentities");
    
    for (int i=0; i<plist_array_get_size(origIdentities); i++) {
        plist_t curr = plist_array_get_item(origIdentities, i);
    
        plist_t cinfo = plist_dict_get_item(curr, "Info");
        plist_t cdevclass = plist_dict_get_item(cinfo, "DeviceClass");
        plist_t cresbeh = plist_dict_get_item(cinfo, "RestoreBehavior");
        
        if (plist_compare_node_value(cresbeh, fresbeh) && plist_compare_node_value(cdevclass, fdevclass)) {
            rt = curr;
            goto error;
        }
    }
    //fails if loop ended without jumping to error
    reterror("found indentity, but failed to match it with orig copy\n");
    
error:
    plist_free(manifest);
    return rt;
#undef reterror
}

void printGeneralBuildIdentityInformation(plist_t buildidentity){
    plist_t info = plist_dict_get_item(buildidentity, "Info");
    plist_dict_iter iter = NULL;
    plist_dict_new_iter(info, &iter);
    
    plist_type t;
    plist_t node = NULL;
    char *key = NULL;
    
    while (plist_dict_next_item(info, iter, &key, &node),node) {
        char *str = NULL;
        switch (t = plist_get_node_type(node)) {
            case PLIST_STRING:
                plist_get_string_val(node, &str);
                printf("%s : %s\n",key,str);
                break;
            case PLIST_BOOLEAN:
                plist_get_bool_val(node, (uint8_t*)&t);
                printf("%s : %s\n",key,((uint8_t)t) ? "YES" : "NO" );
            default:
                break;
        }
        if (str) free(str);
    }
    if (iter) free(iter);
}


int verifyIMG4(char *buf, plist_t buildmanifest){
    int error = 0;
#define reterror(a ...){error=1;error(a);goto error;}
    char *im4pSHA = NULL;
    if (sequenceHasName(buf, "IMG4")){
        //verify IMG4
        char *im4p = getIM4PFromIMG4(buf);
        if (!im4p) goto error;
        im4pSHA = getSHA1ofSqeuence(im4p);
        
        error("THIS FEATURE IS NOT IMPLEMENTED YET\n");
        
#warning TODO IMPLEMENT
        //TODO: set buf to IM4M here
    }
    
    if (!sequenceHasName(buf, "IM4M"))
        reterror("unable to find IM4M tag");
    plist_t identity = getBuildIdentityForIM4M(buf, buildmanifest);
    
    printf("\n");
    if (identity){
        printf("IM4M is valid for the given BuildManifest for the following restore:\n");
        printGeneralBuildIdentityInformation(identity);
        
    }else{
        printf("IM4M is not valid for any restore within the Buildmanifest\n");
        error = 1;
    }
        
    printf("\n");
error:
    safeFree(im4pSHA);
    return error;
#undef reterror
}





