#include "../app/src/main/cpp/game.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#define DT (1.f/60)
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){printf("  FAIL: %s\n",msg);fails++;} else printf("  ok: %s\n",msg);}while(0)

static void place(Game*g,float px,float py,float ex,float ey){ game_reset(g); g->f[0].x=px;g->f[0].y=py;g->f[1].x=ex;g->f[1].y=ey; Input z; memset(&z,0,sizeof z); Input in[2]={z,z}; game_step(g,DT,in);}

int main(void){
 Game g; Input z; memset(&z,0,sizeof z);

 printf("T1: tez turtki (flick) -> zarba tegadi\n");
 place(&g,0,22,0,-2);
 for(int i=0;i<60;i++){ Input in[2]={z,z}; if(i>=5&&i<25) in[0].arm[1][1]=1.f; game_step(&g,DT,in);}
 printf("  dushman hp=%.2f hits=%d\n",g.f[1].hp,g.hits[0]);
 CHECK(g.hits[0]>=1 && g.f[1].hp<98.5f,"flick zarba beradi");

 printf("T2: sekin bosish (1.2s ichida) -> zarba yo'q\n");
 place(&g,0,22,0,-2);
 for(int i=0;i<150;i++){ Input in[2]={z,z}; float r=i/72.f; if(r>1)r=1; in[0].arm[1][1]=r; game_step(&g,DT,in);}
 printf("  dushman hp=%.2f hits=%d\n",g.f[1].hp,g.hits[0]);
 CHECK(g.hits[0]==0 && g.f[1].hp>=99.9f,"sekin siljish zarba emas");

 printf("T3: raqib himoyada (orqa yarim doira) -> zarba bloklanadi\n");
 place(&g,0,22,0,-2);
 for(int i=0;i<60;i++){ Input in[2]={z,z}; in[1].arm[0][0]=.35f;in[1].arm[0][1]=-.95f;in[1].arm[1][0]=-.35f;in[1].arm[1][1]=-.95f; if(i>=10&&i<30) in[0].arm[1][1]=1.f; game_step(&g,DT,in);}
 printf("  dushman hp=%.2f hits=%d blocks=%d\n",g.f[1].hp,g.hits[0],g.blocks[1]);
 CHECK(g.blocks[1]>=1 && g.hits[0]==0 && g.f[1].hp>97,"blok zarbani to'xtatadi");

 printf("T3b: faqat BIR qo'l himoyada: shu tomon bloklanadi\n");
 place(&g,0,22,0,-2);
 for(int i=0;i<60;i++){ Input in[2]={z,z}; in[1].arm[0][0]=.35f;in[1].arm[0][1]=-.95f; if(i>=10&&i<30) in[0].arm[0][1]=1.f; game_step(&g,DT,in);}
 printf("  hits=%d blocks=%d\n",g.hits[0],g.blocks[1]);

 printf("T4: avatarga bog'lanish: raqib chapda, 'oldinga' = raqib tomoni\n");
 place(&g,0,0,-24,0);
 for(int i=0;i<60;i++){ Input in[2]={z,z}; if(i>=5&&i<25) in[0].arm[1][1]=1.f; game_step(&g,DT,in);}
 printf("  hp=%.2f hits=%d\n",g.f[1].hp,g.hits[0]);
 CHECK(g.hits[0]>=1,"raqib chapda bo'lsa ham oldinga zarba tegadi");
 printf("T4b: raqib orqada/yonida emas, ya'ni 'orqaga' joystik zarba bermaydi\n");
 place(&g,0,0,-24,0);
 for(int i=0;i<60;i++){ Input in[2]={z,z}; if(i>=5&&i<25) in[0].arm[1][1]=-1.f; game_step(&g,DT,in);}
 CHECK(g.hits[0]==0,"orqa yarim doira zarba emas");

 printf("T5: qo'lqop joystikka ergashadi (radius: old katta, orqa kichik)\n");
 place(&g,0,22,0,-22);
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[1][1]=1.f; game_step(&g,DT,in);}
 float reachF=g.f[0].glove[1][0];
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[1][1]=-1.f; game_step(&g,DT,in);}
 float backF=g.f[0].glove[1][0];
 printf("  old=%.1f orqa=%.1f\n",reachF,backF);
 CHECK(reachF>20 && backF>3 && backF<6,"old radius katta, orqa radius kichik");

 printf("T6: AI hujum tezligi (60 s, o'yinchi harakatsiz, himoyasiz)\n");
 int strikes[3]; float dmg[3];
 for(int diff=0;diff<3;diff++){
   srand(7); game_reset(&g);
   for(int i=0;i<3600;i++){ Input in[2]={z,z}; ai_input(&g,DT,diff,&in[1]); game_step(&g,DT,in); g.f[0].hp=100;}
   strikes[diff]=g.ai.strikes; dmg[diff]=g.hits[1]*1.f;
   printf("  diff=%d zarbalar=%d tegdi=%d\n",diff,g.ai.strikes,g.hits[1]);
 }
 CHECK(strikes[1]>=1.6f*strikes[0],"2-qiyinlikda hujum ~2x");
 CHECK(strikes[2]>=strikes[1]*0.9f,"3-qiyinlik hujumi 2-qiyinlikdan kam emas");
 CHECK(dmg[0]>0,"AI haqiqatan uradi");

 printf("T7: o'yinchi doim bloklaydi -> AI zarbalari bloklanadi\n");
 srand(7); game_reset(&g);
 for(int i=0;i<3600;i++){ Input in[2]={z,z}; in[0].arm[0][0]=.35f;in[0].arm[0][1]=-.95f;in[0].arm[1][0]=-.35f;in[0].arm[1][1]=-.95f; ai_input(&g,DT,0,&in[1]); game_step(&g,DT,in); }
 printf("  hits=%d blocks=%d hp=%.1f\n",g.hits[1],g.blocks[0],g.f[0].hp);
 CHECK(g.blocks[0]>g.hits[1],"blok ko'pchilik zarbani qaytaradi");

 printf("T8: 3-qiyinlik: AI hujum qilmasa blokda (o'yinchi har 0.6s da zarba)\n");
 float hpLoss[3];
 for(int diff=0;diff<3;diff++){
   srand(11); game_reset(&g);
   for(int i=0;i<3600;i++){ Input in[2]={z,z}; int ph=i%36; int arm=(i/36)&1; if(ph>=3&&ph<12) in[0].arm[arm][1]=1.f; ai_input(&g,DT,diff,&in[1]); game_step(&g,DT,in); g.f[0].hp=100; }
   hpLoss[diff]=100-g.f[1].hp; printf("  diff=%d dushman yo'qotgan hp=%.1f (hits=%d blocks=%d)\n",diff,hpLoss[diff],g.hits[0],g.blocks[1]);
 }
 CHECK(hpLoss[2]<hpLoss[0],"3-qiyinlikda dushmanga zarar kamroq");


 printf("T9: 2 joystik: ikkala oldinga ushlansa -> raqib tomon yuradi, qo'llar himoyada\n");
 place(&g,0,22,0,-22);
 float y0=g.f[0].y;
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[0][1]=1.f; in[0].arm[1][1]=1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 printf("  y: %.1f -> %.1f  yuryapti=%d guard=%.2f\n",y0,g.f[0].y,g.walking[0],g.f[0].guard[0]);
 CHECK(g.f[0].y<y0-5 && g.f[0].guard[0]>0.8f,"oldinga yuradi va himoyada");

 printf("T10: tez 1-2 kombo (qisqa ikkala oldinga) yurishni yoqmaydi\n");
 place(&g,0,22,0,-22); y0=g.f[0].y;
 for(int i=0;i<4;i++){ Input in[2]={z,z}; in[0].arm[0][1]=1.f; in[0].arm[1][1]=1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 for(int i=0;i<20;i++){ Input in[2]={z,z}; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 CHECK(fabsf(g.f[0].y-y0)<0.5f,"0.07 s ushlash yurish emas");

 printf("T11: navbatma-navbat zarba (1-2) yurishga aylanmaydi\n");
 place(&g,0,22,0,-2); y0=g.f[0].y;
 for(int i=0;i<90;i++){ Input in[2]={z,z}; int ph=i%30; if(ph>=2&&ph<10) in[0].arm[(i/30)&1][1]=1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 printf("  hits=%d y=%.1f\n",g.hits[0],g.f[0].y);
 CHECK(g.hits[0]>=2 && fabsf(g.f[0].y-y0)<0.5f,"kombo uradi, yurmaydi");

 printf("T12: ikkala o'ngga -> yonga aylanadi\n");
 place(&g,0,22,0,-22); float x0=g.f[0].x;
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[0][0]=1.f; in[0].arm[1][0]=1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 printf("  x: %.1f -> %.1f\n",x0,g.f[0].x);
 CHECK(g.f[0].x>x0+4,"o'ngga yuradi");

 printf("T13: ikkala orqaga (to'liq) -> orqaga yuradi\n");
 place(&g,0,22,0,-22); y0=g.f[0].y;
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[0][1]=-1.f; in[0].arm[1][1]=-1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 CHECK(g.f[0].y>y0+4,"orqaga yuradi");

 printf("T14: yengil orqaga (50%%) -> joyida himoya, yurmaydi\n");
 place(&g,0,22,0,-22); y0=g.f[0].y;
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[0][1]=-.5f; in[0].arm[1][1]=-.5f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 CHECK(fabsf(g.f[0].y-y0)<0.5f && g.f[0].guard[0]>0.4f,"yengil og'ish = joyida himoya");

 printf("T15: ichkariga (qo'llar bir-biriga) -> joyida blok, yurmaydi\n");
 place(&g,0,22,0,-22); y0=g.f[0].y;
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[0][0]=.6f; in[0].arm[0][1]=-.8f; in[0].arm[1][0]=-.6f; in[0].arm[1][1]=-.8f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 CHECK(fabsf(g.f[0].y-y0)<0.5f && g.f[0].guard[0]>0.9f,"ichkariga = blok, yurish yo'q");

 printf("T16: raqib chapda bo'lsa 'oldinga' = raqib tomoniga yuradi (avatar sistemasi)\n");
 place(&g,0,0,-30,0); x0=g.f[0].x;
 for(int i=0;i<30;i++){ Input in[2]={z,z}; in[0].arm[0][1]=1.f; in[0].arm[1][1]=1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 CHECK(g.f[0].x<x0-4,"chapga (raqib tomon) yuradi");

 printf("T17: yurish tezligi 60%%\n");
 place(&g,0,22,0,-22); y0=g.f[0].y;
 for(int i=0;i<60;i++){ Input in[2]={z,z}; in[0].arm[0][1]=-1.f; in[0].arm[1][1]=-1.f; game_two_stick(&g,0,DT,&in[0]); game_step(&g,DT,in);}
 {float d=g.f[0].y-y0; printf("  1 s da %.1f birlik (kutilgan ~16, ring chegarasigacha)\n",d); CHECK(d>10&&d<19,"tezlik ~60%");}

 printf("T18: regressiya: qo'lqop tanani 'sakrab o'tib' ketmaydi (masofa 12..26 da har zarba tegadi)\n");
 {
   float ds[] = {12,13,14,15,16,17,18,20,22,24,26}; int bad = 0;
   for (int di = 0; di < 11; di++) {
     Game gg; game_reset(&gg);
     for (int k = 0; k < 2; k++) {
       gg.f[0].x = 0; gg.f[0].y = ds[di]/2; gg.f[1].x = 0; gg.f[1].y = -ds[di]/2;
       for (int i = 0; i < 40; i++) { Input in[2]={z,z}; if (i < 5) in[0].arm[k][1] = 1.f; game_step(&gg, DT, in); gg.f[1].x = 0; gg.f[1].y = -ds[di]/2; gg.f[0].x = 0; gg.f[0].y = ds[di]/2; }
     }
     if (gg.hits[0] < 2) { bad++; printf("  masofa %.0f: faqat %d/2 zarba tegdi\n", ds[di], gg.hits[0]); }
   }
   CHECK(bad == 0, "har masofada ikkala qo'lning zarbasi tegadi");
 }

 printf("T19: mehmon prediksiyasi xost simulyatsiyasi bilan bir xil natija beradi\n");
 {
   Game gg; game_reset(&gg); float pg[2][2], pd[2];
   for (int k = 0; k < 2; k++) { pg[k][0] = gg.f[0].glove[k][0]; pg[k][1] = gg.f[0].glove[k][1]; pd[k] = 0; }
   srand(9); float maxd = 0; Input cur; memset(&cur, 0, sizeof cur);
   for (int i = 0; i < 60 * 20; i++) {
     if (i % 17 == 0) for (int k = 0; k < 2; k++) { cur.arm[k][0] = (rand() % 201 - 100) / 100.f; cur.arm[k][1] = (rand() % 201 - 100) / 100.f; }
     Input in[2] = { cur, z };
     game_step(&gg, DT, in);                      // raqib uzoqda: urilish yo'q
     game_predict_arms(pg, pd, &cur, DT);
     for (int k = 0; k < 2; k++) {
       float e = fmaxf(fabsf(pg[k][0] - gg.f[0].glove[k][0]), fmaxf(fabsf(pg[k][1] - gg.f[0].glove[k][1]), fabsf(pd[k] - gg.f[0].guard[k])));
       if (e > maxd) maxd = e;
     }
   }
   printf("  eng katta farq = %.6f\n", maxd);
   CHECK(gg.hits[0] == 0 && maxd < 1e-4f, "20 s tasodifiy kirishda prediksiya xostdagi qo'lqop va himoya bilan mos");
 }

 printf("T20: himoya charchog'i (qo'shimcha qiyinlik)\n");
 {
   Input gin; memset(&gin, 0, sizeof gin);
   gin.arm[0][0] = .35f; gin.arm[0][1] = -.95f; gin.arm[1][0] = -.35f; gin.arm[1][1] = -.95f;
   // 1) o'chiq: uzoq ushlansa ham doira o'zgarmaydi
   Game a; game_reset(&a); a.fatigue = 0;
   for (int i = 0; i < 60 * 8; i++) { Input in[2] = { z, gin }; game_step(&a, DT, in); }
   CHECK(a.f[1].gscale[0] == 1.f && a.f[1].gscale[1] == 1.f, "qoida o'chiq: 8 s ushlansa ham himoya doirasi o'zgarmaydi");
   // 2) yoqiq: vaqt profili
   Game b; game_reset(&b); b.fatigue = 1; float s29 = 0, s40 = 0, s52 = 0;
   for (int i = 0; i < 60 * 6; i++) { Input in[2] = { z, gin }; game_step(&b, DT, in);
     if (i == (int)(2.9f * 60)) { s29 = b.f[1].gscale[0]; }
     if (i == 4 * 60) { s40 = b.f[1].gscale[0]; }
     if (i == (int)(5.2f * 60)) { s52 = b.f[1].gscale[0]; } }
   printf("  doira ko'paytiruvchisi: 2.9 s=%.2f  4.0 s=%.2f  5.2 s=%.2f\n", s29, s40, s52);
   CHECK(s29 > 0.99f, "3 soniyagacha himoya to'liq");
   CHECK(s40 > 0.45f && s40 < 0.65f, "4 soniyada ~yarmiga kichraygan (3 s dan keyin 2 s da silliq)");
   CHECK(fabsf(s52 - 0.1f) < 0.01f && fabsf(b.f[1].gscale[0] - 0.1f) < 0.01f, "5 soniyada 10 baravar kichik (0.1) va shundayligicha qoladi");
   // 3) qo'llar alohida
   Game c2; game_reset(&c2); c2.fatigue = 1; Input one; memset(&one, 0, sizeof one); one.arm[0][0] = .35f; one.arm[0][1] = -.95f;
   for (int i = 0; i < 60 * 6; i++) { Input in[2] = { z, one }; game_step(&c2, DT, in); }
   CHECK(fabsf(c2.f[1].gscale[0] - 0.1f) < 0.01f && c2.f[1].gscale[1] == 1.f, "faqat ushlangan qo'l charchaydi, ikkinchisi to'liq");
   // 4) kichraygan himoya blokni o'tkazib yuboradi, yangisi bloklaydi
   int blocks_early = 0, hits_late = 0;
   { Game e; game_reset(&e); e.fatigue = 1; e.f[0].x = 0; e.f[0].y = 22; e.f[1].x = 0; e.f[1].y = -2;
     for (int i = 0; i < 90; i++) { Input in[2] = { z, gin }; if (i >= 30 && i < 40) in[0].arm[1][1] = 1.f; game_step(&e, DT, in); }
     blocks_early = e.blocks[1]; }
   { Game e; game_reset(&e); e.fatigue = 1; e.f[0].x = 0; e.f[0].y = 22; e.f[1].x = 0; e.f[1].y = -2;
     for (int i = 0; i < 60 * 8; i++) { Input in[2] = { z, gin }; if (i >= 60 * 6 && i < 60 * 6 + 10) in[0].arm[1][1] = 1.f; game_step(&e, DT, in); }
     hits_late = e.hits[0]; }
   printf("  erta zarba: bloklandi=%d | 6 s dan keyingi zarba: tegdi=%d\n", blocks_early, hits_late);
   CHECK(blocks_early >= 1, "yangi himoya zarbani bloklaydi");
   CHECK(hits_late >= 1, "6 s ushlangan (kichraygan) himoya zarbani o'tkazib yuboradi");
   // 5) tiklanish
   Game r; game_reset(&r); r.fatigue = 1;
   for (int i = 0; i < 60 * 6; i++) { Input in[2] = { z, gin }; game_step(&r, DT, in); }
   float before = r.f[1].gscale[0];
   for (int i = 0; i < 60 * 3; i++) { Input in[2] = { z, z }; game_step(&r, DT, in); }
   printf("  6 s ushlangach 3 s bo'shatildi: %.2f -> %.2f\n", before, r.f[1].gscale[0]);
   CHECK(before < 0.11f && r.f[1].gscale[0] > 0.99f, "himoya bo'shatilgach doira tiklanadi");
   // qisqa uzilish (0.5 s) charchoqni butunlay bekor qilmaydi
   Game q; game_reset(&q); q.fatigue = 1;
   for (int i = 0; i < 60 * 4; i++) { Input in[2] = { z, gin }; game_step(&q, DT, in); }
   for (int i = 0; i < 30; i++) { Input in[2] = { z, z }; game_step(&q, DT, in); }
   for (int i = 0; i < 60 * 2; i++) { Input in[2] = { z, gin }; game_step(&q, DT, in); }
   CHECK(q.f[1].gscale[0] < 0.9f, "0.5 s ga bo'shatib yana ushlash charchoqni butunlay bekor qilmaydi");
 }
 printf("%s (%d xato)\n",fails?"XATO BOR":"HAMMASI O'TDI",fails);
 return fails;
}
