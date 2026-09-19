# Fady Arena Mobile Client

كلاينت مستقل لـiPhone وAndroid مبني بـGodot 4، ويتصل بسيرفر اللعبة الحالي بدون أي تعديل في السيرفر أو نسخة سطح المكتب.

## السيرفر المضبوط افتراضيًا

```text
ws://158.220.122.81:8089/game
```

يمكن تغييره من `server_config.json` من غير تعديل الكود:

```json
{
  "server_url": "ws://158.220.122.81:8089/game",
  "player_name": "Fady Mobile"
}
```

## ما يعمل في نسخة الموبايل

- اتصال بنفس بروتوكول WebSocket JSON الموجود في سيرفر Node.js.
- إنشاء Room، عرض الغرف المتاحة، ودخول Room.
- خمس جولات، وصحة وضرر وموت محسوبون من السيرفر.
- ترتيب أسلحة عشوائي قادم من السيرفر.
- الحواجز تستخدم `obstacleSeed` القادم من السيرفر.
- عالم ثلاثي الأبعاد وشخصيتان كرتونيتان.
- عصا حركة لمس، Jump، Crouch، Attack، وأزرار للأسلحة المفتوحة.
- يدعم لوحة المفاتيح داخل Godot للتجربة على Mac/Windows.
- مكتوب داخل اللعبة `Developed by Fady Gamil`.

## فتح المشروع

1. نزّل Godot 4.3 أو أحدث مع Export Templates.
2. افتح Godot ثم `Import`.
3. اختر ملف `project.godot`.
4. شغّل المشروع داخل المحرر أولًا للتأكد من الاتصال بالسيرفر.

## تصدير Android

1. ثبّت Android Studio، Android SDK، JDK 17، وGodot Export Templates.
2. في Godot افتح `Editor Settings > Export > Android` وحدد مسارات Java وAndroid SDK.
3. افتح `Project > Export > Android`.
4. اختر `Export Project` لإنتاج APK.
5. للسيرفر الحالي يجب السماح باتصال الإنترنت وcleartext `ws://`؛ إعدادات المشروع مهيأة لذلك.

## تصدير iPhone

1. يلزم جهاز Mac عليه Xcode وحساب Apple Developer للتشغيل على iPhone حقيقي.
2. افتح `Project > Export > iOS`.
3. ضع Apple Team ID بدل `CHANGE_TO_YOUR_APPLE_TEAM_ID`.
4. صدّر المشروع إلى مجلد جديد.
5. افتح ملف Xcode الناتج، اختر iPhone وفريق التوقيع، ثم اضغط Run.
6. تمت إضافة استثناء ATS لأن السيرفر يستخدم `ws://` غير المشفر عمدًا للتجربة التعليمية.

## تنبيه

اتصال `ws://` واضح وغير مشفر ومناسب لمعمل الدراسة فقط. قبل نشر اللعبة للعامة يجب استخدام `wss://` وشهادة TLS.

