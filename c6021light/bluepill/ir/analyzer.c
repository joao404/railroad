// analyse.cpp : Auswerteteil für die Konsolenanwendung zur Analyse des Digitalsignals.
// C 2005 - 2019 Rainer Müller 
// Das Programm unterliegt den Bedingungen der GNU General Public License 3 (GPL3).


#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "analyzer.h"
#include "mmadr.h"

struct st_mm { int strt, pause, adr, fkt, dat, xdat; bool freq2; } mmdat, mmaltdat;
struct st_dc { int strt, pre, daten[8]; } dcdat;

bool mfxdetail, mfxread = false;
unsigned int altduration, dcccounter, mfxcounter, mfxbits, paar, prea, acounter;
char mfxdaten[128], mfxzeile[8192];

void mm_print(void) {
        char half = ',';
        if ((mmdat.adr == mmaltdat.adr) && (mmdat.freq2 == mmaltdat.freq2) &&
                (mmdat.fkt == mmaltdat.fkt) && (mmdat.dat == mmaltdat.dat) &&
                (mmdat.xdat == mmaltdat.xdat))  printf(" <REP>");
        else {
                mmaltdat = mmdat;
                if (mmdat.freq2) { 
                        int addr = mm_adrtab[mmdat.adr];
                        printf("\n %6d ms: MMD A=%3d => ", mmdat.strt, addr);
                        if (mmdat.fkt == 0) {
                                if (addr == 80) addr = 0;
                                addr = addr * 4 + ((mmdat.dat >> 1) & 3) + 1;
                                printf("ACC %3d, P%d = %d ", addr, mmdat.dat & 1, mmdat.dat >> 3);
                        }
                        else {
                                printf("FKT F=%1d, D=%2d ", mmdat.fkt, mmdat.dat);
                        }
                }
                else if (mmdat.dat == mmdat.xdat) {
                        printf("\n %6d ms: MM1 ", mmdat.strt);
                        printf("A=%3d, F=%1d, D=%2d ", mm_adrtab[mmdat.adr], mmdat.fkt, mmdat.dat);     }
                else {
                        if ((mmdat.fkt == 1) || (mmdat.fkt == 2)) half = '+';
                        printf("\n %6d ms: MM2 ", mmdat.strt);
                        printf("A=%3d, F=%1d, D=%2d%c X=%2d ", 
                                mm_adrtab[mmdat.adr], mmdat.fkt, mmdat.dat, half, mmdat.xdat);
                        if (((mmdat.xdat == 5) && (mmdat.dat < 8)) || ((mmdat.xdat == 10) && (mmdat.dat > 7)))
                                mmdat.xdat = mmdat.dat;
                        switch (mmdat.xdat) {
                                case  2:  case 10:      printf("V     ");       break;
                                case  3:                        printf("F1 aus");       break;
                                case  4:                        printf("F2 aus");       break;
                                case  5:  case 13:      printf("R     ");       break;
                                case  6:                        printf("F3 aus");       break;
                                case  7:                        printf("F4 aus");       break;
                                case 11:                        printf("F1 ein");       break;
                                case 12:                        printf("F2 ein");       break;
                                case 14:                        printf("F3 ein");       break;
                                case 15:                        printf("F4 ein");       break;
                        }
                }
        }
}

void dcc_cv_acc(int s)
{
        int cvnr = 0;
        if ((dcdat.daten[s] & 0xF0) == 0xF0) {
                int b1 = dcdat.daten[s + 1];
                switch (dcdat.daten[s] & 15) {
                case 2: cvnr = 23;      break;
                case 3: cvnr = 24;      break;
                }
                printf(" Short Form CV %d  = %2x", cvnr, b1);
        }
        else {
                cvnr = (dcdat.daten[s] & 3) * 256 + dcdat.daten[s + 1] + 1;
                int b2 = dcdat.daten[s + 2];
                printf(" CV = %d, ", cvnr);
                switch (dcdat.daten[s] & 12) {
                case  0:        printf("READ 4B");      break;
                case  4:        printf("VER %2x", b2);  break;
                case  8:        printf("BIT %d ", b2 & 7);
                        if (b2 & 16) printf("WRI %d", (b2 >> 3) & 1);
                        else printf("VER %d", (b2 >> 3) & 1);
                        break;
                case 12:        printf("WRI %2x", b2);  break;
                }
        }
}

/*
basic           0 10AAAAAA 0 1AAACDDD 0                         (DDD => AAP)
extended        0 10AAAAAA 0 0AAA0AA1 0 000XXXXX 0
*/
void dcc_accessory(void)
{
        int addr;
        addr = ((~dcdat.daten[1] << 4) & 0x700) + ((dcdat.daten[0] << 2) & 0xFC) +
                                                                                        ((dcdat.daten[1] >> 1) & 3) + 1;
        if (addr > 2044) printf(" *BC*");
        if (dcdat.daten[1] & 0x80) {
                printf(" BASACC %3d, P%d = %d ", addr,
                                        dcdat.daten[1] & 1, (dcdat.daten[1] >> 3) & 1);
        }
        else {
                printf(" EXTACC %3d, => %d ", addr, dcdat.daten[2]  & 0x1F);
        }
}

void dcc_decode(void)
{
        unsigned int i, bef = 0;
        char c;

        if (dcdat.daten[0] == 0) {
                printf(" *BC*");
                bef = 1;        }
        else if (dcdat.daten[0] == 255) {
                printf(" *IDLE*");
                return;
        }
        else if ((dcdat.daten[0] >= 1) && (dcdat.daten[0] <= 127)) {
                printf("  K%3d", dcdat.daten[0]);
                bef = 1;        }
        else if ((dcdat.daten[0] >= 192) && (dcdat.daten[0] <= 231)) {
                printf("  L%3d", (dcdat.daten[0] - 192) * 256 + dcdat.daten[1]);
                bef = 2;        } 
        else if (dcdat.daten[0] < 192) {
                dcc_accessory();
                return;
                }
        for (i=bef; i<((dcccounter - 6) / 18); i++) {
                c = (dcdat.daten[i] & 16) ? '+' : ' ';
                switch (dcdat.daten[i] >> 5) {
                case 0: switch (dcdat.daten[i]) {                                                       // Decoder and Consist Control Instruction
                                case 0:         printf(" RESET");               break;
                                case 1:         printf(" HARD RESET");  break;
                                default:        printf(" CC ??");
                                }
                                break;
                case 1: if (dcdat.daten[i+1] & 0x80)                                            // Advanced Operation Instructions
                                                printf(" ADV:V%d", dcdat.daten[i+1] & 127);
                                else    printf(" ADV:R%d", dcdat.daten[i+1] & 127);
                                i++;    return;
                case 2: printf(" S+D:R %d%c", dcdat.daten[i] & 15, c);  break;  // Speed and Direction Instruction for reverse operation
                case 3: printf(" S+D:V %d%c", dcdat.daten[i] & 15, c);  break;  // Speed and Direction Instruction for forward operation
                case 4: printf(" FG1:%x", dcdat.daten[i] & 31);         break;  // Function Group One Instruction
                case 5: c = (dcdat.daten[i] & 16) ? 'A' : 'B';                          // Function Group Two Instruction
                                printf(" FG2%c:%x", c, dcdat.daten[i] & 15);break;
                case 6: switch (dcdat.daten[i] & 31) {                                          // Future Expansion
                                case 30:        printf(" FG3:%x", dcdat.daten[i+1]);
                                                        i++;    break;
                                case 31:        printf(" FG4:%x", dcdat.daten[i+1]);
                                                        i++;    break;
                                default:        printf(" FE ??");
                                }
                                break;
                case 7: printf(" POM");                                                                         // Configuration Variable Access Instruction
                                dcc_cv_acc(i);  i += 2;                                         break;
        }       }
}

void dcc_service(void)
{
        if (dcccounter > 67) {
                printf(" *SM DIR*");
                dcc_cv_acc(0);
        }
        else {
                printf(" *SM PHY* REG = %d ", dcdat.daten[0] & 7);
                if (dcdat.daten[0] & 8) printf("WRI %x", dcdat.daten[1]);
                else printf("VER %x", dcdat.daten[1]);
        }
}

void dcc_analyzer(void)
{
        int i, sm, chk = 0;
        if (++dcccounter & 1) return;
        int bp = (dcccounter - 6) / 2;
        if ((bp % 9) == 8) {
                if (paar < 16) {
                        printf("\n %6d ms: DCC Pr.%2d, Daten:", dcdat.strt, dcdat.pre/2);
                        for (i=0; i<=(bp/9); i++) {
                                printf(" %2.2x", dcdat.daten[i]);
                                if (bp/9 > i) chk ^= dcdat.daten[i];
                                else if (chk == dcdat.daten[i]) {
                                        printf("(OK)");
                                        sm = (dcdat.daten[0] < 128) & (dcdat.daten[0] >= 112);
                                        if (dcdat.pre < 40) sm = 0;
                                        if (sm) dcc_service();
                                        else dcc_decode();
                        }       }
                        for (i=0; i<=(bp/9); i++) dcdat.daten[i] = 0;
                        dcccounter = 0;
                        acounter = 0;
                        dcdat.pre = 0;
                        mmaltdat.adr = -1;
        }       }
        else dcdat.daten[bp/9] += (!(paar > 15)) << (7-(bp % 9));
}

unsigned int mfxwert(int start, int laenge)
{
        unsigned int w = 0;
        for (int i=start; i<(start+laenge); i++)
                w += w + (mfxdaten[i] & 1);
        return w;
}

unsigned long mfxlwert(int start, int laenge)
{
        unsigned long w = 0;
        for (int i=start; i<(start+laenge); i++)
                w += w + (mfxdaten[i] & 1);
        return w;
}

unsigned int mfx_crc(void)
{
        int cr = 127;
        for (unsigned int i=0; i<(mfxbits); i++) {
                cr = (cr << 1) + (mfxdaten[i] - 0x30);
                if (cr & 0x100) cr ^= 7;
        }
        return (cr & 0xff);
}

bool mfx_print(void)
{
        unsigned int i, f, mfx1 = 0, strtoffset = 0, start;
        char c;
        if (mfxcounter <= 1) return false;
        memset(mfxzeile+mfxcounter-1, 0, 2);
        if (mfxzeile[2] != 'k') {                               // Erkennen von Taktimpulsen mit großem Abstand
                if (!mfxread) return false;
                if (mfxcounter < 4) {
                        printf("P");
                        return true;
                }
        }
        if (mfxdetail) printf("\n %3d mfx-Bits: %s", mfxcounter-2, &mfxzeile[1]);
        f = 0;
        for (i=1; i<(mfxcounter-1); i++) {              // Flagerkennung und Separieren wenn mehrere Pakete
                if ((mfxzeile[i] == 'k') && (mfxzeile[i-1] == '0') &&
                        ((mfxzeile[i+1] == '0') | (mfxzeile[i+1] == 0))) memset(mfxzeile+i-1, '_', 3);
        }

        for (i=1; i<(mfxcounter-1); i++) {
                if ((c = mfxzeile[i]) != '_') {
                        if (mfxbits == 0) strtoffset = (i + 6) / 12;
                        if (c == '1') mfx1++;                   // Fülleinsen beseitigen
                        if (((mfx1 != 8) || (c != '0')) && (mfxbits < sizeof(mfxdaten)-1))
                                mfxdaten[mfxbits++] = c;
                        if (c == '0') mfx1 = 0;
                        if (c == 'k') {                                 // Erkennen von Taktimpulsen mit kleinem Abstand
                                printf("p");
                                f = 1;
                        }
                        else f = 0;
                }
                else if (f == 0) {
                        if (mfxbits) printf("\n %6d ms: MFX ", mmdat.strt + strtoffset);
                        mfxdaten[mfxbits] = 0;
                        mfx1 = 0;
                        start = 0;
                        mfxread = false;
                        if (mfxdaten[0] && (mfxdaten[0] < 0x40) && mfx_crc()) printf(" CRC-Fehler ");
                        else while ((start < 128) && (start < (mfxbits - 13)) && (mfxdaten[start] < 0x40)) {
                                int typ = mfxwert(start, 4);
                                if (start == 0) switch (typ) {

                                                /* Adresse */
                                  case  8: case 9:  case 10: case 11:
                                                        printf("A07:%d ", mfxwert(start+2, 7));
                                                        start += 9;             break;
                                  case 12: case 13:
                                                        printf("A09:%d ", mfxwert(start+3, 9));
                                                        start += 12;    break;
                                  case 14:      printf("A11:%d ", mfxwert(start+4, 11));
                                                        start += 15;    break;
                                  case 15:      printf("A14:%d ", mfxwert(start+4, 14));
                                                        start += 18;            break;
                                  default:      printf("Unbekanntes Adressformat %X ", typ);
                                                        start = 999;
                                }
                                else switch (typ) {

                                                /* Richtung und Geschwindigkeit */
                                  case  0:      printf("V3:%d ", mfxwert(start+4, 3));
                                                        start += 7;             break;
                                  case  1:      printf("R3:%d ", mfxwert(start+4, 3));
                                                        start += 7;             break;
                                  case  2:      printf("V7:%d ", mfxwert(start+4, 7));
                                                        start += 11;    break;
                                  case  3:      printf("R7:%d ", mfxwert(start+4, 7));
                                                        start += 11;    break;

                                                /* Funktionen */
                                  case  4:
                                  case  5:      printf("F04:%X ", mfxwert(start+3, 4));
                                                        start += 7;     break;
                                  case  6:      printf("F08:%X ", mfxwert(start+4, 8));
                                                        start += 12;    break;
                                  case  7:      printf("F16:%X ", mfxwert(start+4, 16));
                                                        start += 20;    break;
                                  case  8:
                                  case  9:      printf("Func %d auf %d ", mfxwert(start+3, 7), mfxdaten[start+11] & 1);
                                                        start += 12;    break;

                                                /* Identifizierung */
                                  case 14:      switch (mfxwert(start+4, 2)) {
                                                                case 0: mfxdaten[mfxbits-8] = 0;
                                                                                printf("READ CVS 0x%03X ab Element 0x%02X %d Byte(s)",
                                                                                        mfxwert(start+6, 10), mfxwert(start+16, 6), 1<<mfxwert(start+22, 2));
                                                                                mfxread = true;
                                                                                start += 24;
                                                                                break;
                                                                case 1: mfxdaten[mfxbits-8] = 0;
                                                                                printf("PROG CVS 0x%03X, Element 0x%02X auf Wert 0x%02X",
                                                                                        mfxwert(start+6, 10), mfxwert(start+16, 6), mfxwert(start+24, 8));
                                                                                start += 32;
                                                                                break;
                                                                case 2: printf("Such mit %d Bits von Id-Maske 0x%lX ",
                                                                                        mfxwert(start+6, 6), mfxlwert(start+12, 32));
                                                                                start += 44;
                                                                                break;
                                                                case 3: printf("Neue Adresse %d fuer Decoder mit Id 0x%lX ",
                                                                                        mfxwert(start+6, 14), mfxlwert(start+20, 32));
                                                                                start += 52;
                                                                                break;
                                                        }
                                                        break;
                                  case 15:      switch (mfxwert(start+4, 2)) {
                                                                case 0: printf("Ping an Decoder 0x%lX", mfxlwert(start+6, 32));
                                                                                start += 38;
                                                                                break;
                                                                case 1: printf("Bake von Zentrale 0x%X, Opt 0x%X ",
                                                                                        mfxwert(start+6, 32), mfxwert(start+38, 16));
                                                                                start += 54;
                                                                                break;
                                                                default:printf("Unbekannter Subtyp %d von Typ 15 ",
                                                                                        mfxwert(start+4, 2));
                                                                                start = 999;
                                                        }
                                                        break;
                                  default:      printf("Unbekannter Typ %X ", typ);
                                                        start = 999;
                                }
                        }
                        if (start && (start != (mfxbits - 8)))
                                printf("Laengenfehler S %d, B %d        \n%s_\n", start, mfxbits, mfxdaten);
                        mfxbits = 0;
                        f = 1;  }
        }

        mfxdaten[mfxbits] = 0;
        if (mfxdaten[0]) printf("  +%s+", mfxdaten);
        mfxbits = 0;
        return true;
}


void mfx_analyzer(int duration)
{
        char c = (duration > 68) ? '0' : 'k';
        if ((c == 'k') && (mfxzeile[mfxcounter-2] == 'k')) {
                c = '1';
                mfxcounter--;   }
        mfxzeile[mfxcounter-1] = c;

        if (mfxcounter < 8180) mfxcounter++;
}

void analyzer(int start, int duration) {
    if (duration > 510) {
	if (acounter == 37)
	    mm_print();
	else if (acounter) {
	    if (!mfx_print()) {
		printf("\n %6d ms: ??? mit %3d Wechseln !", mmdat.strt, acounter - 1);
	    }
	    mmaltdat.adr = -1;
	}
	mmdat.strt = start;
	mmdat.pause = duration;
	mmdat.adr = 0;
	mmdat.fkt = 0;
	mmdat.dat = 0;
	mmdat.xdat = 0;
	prea = 0;
	dcdat.pre = 0;
	acounter = 1;
	dcccounter = 0;
	mfxcounter = 1;
    } else if (duration < 90) {
	if (duration > 34)
	    prea++;
    } else {
	if (prea > 20) {
	    dcdat.pre = prea;
	    dcccounter = 2;
	    dcdat.strt = start - 1;
	}
	prea = 0;
    }
    paar = altduration + duration;

    if (dcccounter)
	dcc_analyzer();
    if (mfxcounter)
	mfx_analyzer(duration);
    altduration = duration;

    if (acounter == 0)
	return;
    if (acounter == 3)
	mmdat.freq2 = (paar < 170);
    if ((++acounter & 1) == 0)
	return;
    if (acounter < 2)
	return;
    if (acounter <= 17)
	mmdat.adr += ((duration > 56) << ((acounter - 3) / 2));
    else if (acounter <= 21)
	mmdat.fkt += ((duration > 56) << ((acounter - 19) / 2));
    else
	switch (acounter) {
	case 23:
	case 27:
	case 31:
	case 35:
	    mmdat.dat += ((duration > 56) << ((acounter - 23) / 4));
	    break;
	case 25:
	case 29:
	case 33:
	case 37:
	    mmdat.xdat += ((duration > 56) << ((acounter - 25) / 4));
	    break;
	}
}