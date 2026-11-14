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
 CHECK(g.hits[0]>=1 && g.f[1].hp<95,"flick zarba beradi");

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
 printf("%s (%d xato)\n",fails?"XATO BOR":"HAMMASI O'TDI",fails);
 return fails;
}
