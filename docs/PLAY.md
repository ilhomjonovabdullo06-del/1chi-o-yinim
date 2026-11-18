# Google Play'ga joylash - qadamma-qadam (shoshilinch emas)

Hozirgi holat: loyiha API 36 (Android 16) ga mo'ljallangan va Play talablariga tayyor. Qolgan ishlar - akkaunt, kalit va do'kon formasi.

## 0. Bilib qo'ying
- Google Play dasturchi akkaunti: bir martalik to'lov (hozir 25 dollar). To'lov uchun xalqaro Visa/Mastercard kerak bo'lishi mumkin - ro'yxatdan o'tishdan oldin Play Console'dagi shartlarni o'qing.
- Yangi SHAXSIY akkauntlar ommaviy chiqarishdan oldin yopiq sinovdan o'tishi kerak (taxminan 12 sinovchi, 14 kun). Aniq talabni Play Console'da tekshiring.
- applicationId (hozir `io.github.ilhomjonovabdullo06.boks`) birinchi yuklashdan keyin O'ZGARMAYDI. O'zgartirmoqchi bo'lsangiz `app/build.gradle` dagi `applicationId` ni yuklashdan OLDIN o'zgartiring. `com.example...` qabul qilinmaydi.

## 1. Maxfiylik siyosati manzili (GitHub Pages, tekin)
1. `docs/privacy.html` ichidagi `[SHU YERGA O'Z EMAILINGIZNI YOZING]` va `[PUT YOUR EMAIL HERE]` ni almashtiring.
2. Repo -> Settings -> Pages -> Source: "Deploy from a branch", branch `main`, papka `/docs` -> Save.
3. Manzil: `https://ilhomjonovabdullo06-del.github.io/1chi-o-yinim/privacy.html` (taxminan 1-2 daqiqadan keyin ochiladi). Shu manzilni Play Console'ga kiriting.

## 2. Imzo kaliti (bir marta)
Kalit yo'qolsa ilovani yangilab bo'lmaydi (Play App Signing bilan Google qayta tiklashga yordam beradi, lekin ovora). 
1. Repo'ni PRIVATE (yopiq) qiling: Settings -> General -> Danger Zone -> Change visibility.
2. Actions -> "Imzo kaliti yaratish" -> Run workflow.
3. Tugagach `boks-kalit-MAXFIY` artefaktini yuklab oling va ARTEFAKTNI DARHOL o'chiring. 3 faylni (`boks-upload.jks`, `KEYSTORE_BASE64.txt`, `PAROLLAR.txt`) xavfsiz joyda (parol menejeri/flesh) saqlang.
4. Settings -> Secrets and variables -> Actions -> New repository secret: 
   - `KEYSTORE_BASE64` = KEYSTORE_BASE64.txt ichidagi matn
   - `KEYSTORE_PASSWORD`, `KEY_PASSWORD`, `KEY_ALIAS` = PAROLLAR.txt dagi qiymatlar

## 3. AAB qurish
Actions -> "Release AAB (Google Play uchun)" -> Run workflow. Tugagach `boks-aab` artefaktidan `app-release.aab` ni yuklab oling. Har qurishda versiyaCode avtomatik oshadi.

## 4. Play Console
1. Ilova yarating (til: O'zbekcha yoki Inglizcha; "Game"; "Free").
2. Store listing: matnlar va grafika `docs/STORE_LISTING.md` va `store-assets/` da.
3. App content (formalar):
   - Privacy policy: 1-qadamdagi manzil.
   - Ads: "No ads".
   - App access: cheklov yo'q (hamma funksiya ochiq).
   - Target audience: 13+ (bolalar uchun emas, Families siyosati qo'llanmasin).
   - Content rating (IARC): fantastik/multfilm uslubidagi boks (qon yo'q) - savollarga rost javob bering.
   - Data safety: ilova ma'lumot YIG'MAYDI va ulashmaydi. Ikki telefon orasida faqat tasodifiy laqab va o'yin harakatlari yuboriladi (bizning serverimizga emas). Google ta'rifini o'qib, o'zingiz javob bering.
4. Testing -> Internal testing: AAB'ni yuklang, o'zingiz sinab ko'ring. So'ng (talab bo'lsa) Closed testing, keyin Production.

## 5. Yangilashlar
Kodni o'zgartiring -> `git push` -> "Release AAB" ni qayta ishga tushiring -> yangi AAB'ni Play Console'ga yuklang.

## 6. Kelajakdagi talablar (ESLATMA)
- API 37 ga o'tishda (Play talabi 2027) mahalliy tarmoq uchun ACCESS_LOCAL_NETWORK ruxsati va "orqaga" tugmasi uchun yangi API kerak bo'ladi (`BoksActivity.java` orqali). AndroidManifest.xml dagi izohlarga qarang.
