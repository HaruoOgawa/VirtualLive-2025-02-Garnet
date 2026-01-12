# VirtualLive-2025-02-Garnet
[![IMAGE ALT TEXT HERE](https://github.com/user-attachments/assets/3c7861a1-6525-4e6e-8c9a-79b1bde59e95)](https://www.youtube.com/watch?v=_BYrbOqzhfI)
## はじめに
権利関係の3Dモデル、音声、モーションは一切gitで管理していないので、もし鑑賞される際は公式配布元より購入・ダウンロードをお願いします。
## 導入手順
1. 以下のコマンドを実行
```
mkdir LiveApp
cd LiveApp
git clone https://github.com/HaruoOgawa/Garnet.git
cd Garnet
git checkout 0.2.0
cd ../
git clone https://github.com/HaruoOgawa/VirtualLive-2025-02-Garnet.git

```
2. https://aps.autodesk.com/developer/overview/fbx-sdk からWindows VS2022用のFBX SDKのインストーラーを取得しインストール
3. [Garnet/Src/Library/FBXSDK](https://github.com/HaruoOgawa/Garnet/tree/0.2.0/Garnet/Src/Library/FBXSDK) に2020.3.7フォルダの中身をコピー
<img width="600" height="357" alt="image" src="https://github.com/user-attachments/assets/78e3806d-5295-44c4-9f59-a0d8325dce3a" />

4. https://bowlroll.net/file/155105 のPMXをダウンロード
5. zipファイルを解凍しフォルダ名を「Sour式鏡音リンVer.2.01 → sour_rin」「Sour式鏡音レンVer.2.01 → sour_len」に変更
6. VirtualLive-2025-02-Garnetリポジトリ内のResources/Untrackedフォルダに画像のような配置でコピーする
<img width="295" height="99" alt="{E419A31A-E8E9-414D-BFEF-A81F6449081C}" src="https://github.com/user-attachments/assets/633f3c20-1358-4903-b003-59aa355c6765" />

7. https://bowlroll.net/file/157549 からVMDをダウンロードし「IA.vmd」を「VirtualLive-2025-02-Garnet\Resources\User\Untracked」にコピー
8.  (任意) [リンク]([https://www.youtube.com/watch?v=xIpIbc7IEWo](https://music.apple.com/jp/song/%E5%85%AD%E5%85%86%E5%B9%B4%E3%81%A8%E4%B8%80%E5%A4%9C%E7%89%A9%E8%AA%9E-feat-ia/1605307547))より曲を購入。"VirtualLive-2025-02-Garnet\Resources\User\Untracked\Rokuchonen.wav"として配置
9. VirtualLive-2025-02-Garnet\VirtualLive-2025-02-Garnet.slnをVisual Studio 2022で開き、ソリューション構成をReleaseGLFWとしてビルドする
10. ビルドしてできたx64/ReleseGLFWにResourcesフォルダをコピー
11. [Processing.NDI.Lib.x64.dll](https://github.com/HaruoOgawa/Garnet/blob/0.2.0/Garnet/Src/Library/NDI/Bin/x64/Processing.NDI.Lib.x64.dll) をx64/ReleseGLFWにコピー
12. (任意) x64\ReleaseGLFW\Resources\User\DMXSender のTouchDesinerを開く、「1, 2, 3 ,4 ,5 ,6 , 7, 8, 9, 1, 4」の順番にキーを入力してDMX操作を初期化する
13. (任意) x64\ReleaseGLFW\Resources\User\NDIMovie\Haru86_GraphicsArt01.exe を実行。これでNDIの映像を送信します
14. x64\ReleaseGLFW\VirtualLive-2025-02-Garnet.exeを実行
