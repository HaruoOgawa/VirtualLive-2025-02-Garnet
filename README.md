# VirtualLive-2025-02-Garnet
[![IMAGE ALT TEXT HERE](https://github.com/user-attachments/assets/3c7861a1-6525-4e6e-8c9a-79b1bde59e95)](https://www.youtube.com/watch?v=_BYrbOqzhfI)
## はじめに
権利関係の3Dモデル、音声、モーションは一切gitで管理していないので、もし鑑賞される際は公式配布元より購入・ダウンロードをお願いします。
## 導入手順
1. [Garnet ver 0.20](https://github.com/HaruoOgawa/Garnet/releases/tag/0.20)をクローンもしくはダウンロード
2. このリポジトリをGarnetと同じディレクトリにクローンもしくはダウンロード
3. https://bowlroll.net/file/155105 のPMXをダウンロード
4. zipファイルを解凍しフォルダ名を「Sour式鏡音リンVer.2.01 → sour_rin」「Sour式鏡音レンVer.2.01 → sour_len」に変更
リポジトリ内のResourcesフォルダに画像のような配置でコピーする
<img width="295" height="99" alt="{E419A31A-E8E9-414D-BFEF-A81F6449081C}" src="https://github.com/user-attachments/assets/633f3c20-1358-4903-b003-59aa355c6765" />
5. https://bowlroll.net/file/157549 からVMDをダウンロードし「IA.vmd」を「VirtualLive-2025-02-Garnet\Resources\User\Untracked」にコピー
6.  (任意) [リンク]([https://www.youtube.com/watch?v=xIpIbc7IEWo](https://music.apple.com/jp/song/%E5%85%AD%E5%85%86%E5%B9%B4%E3%81%A8%E4%B8%80%E5%A4%9C%E7%89%A9%E8%AA%9E-feat-ia/1605307547))より曲を購入。"VirtualLive-2025-02-Garnet\Resources\User\Untracked\Rokuchonen.wav"として配置
7. ソリューション構成をReleaseGLFWでビルドする
8. ビルドしてできたx64/ReleseGLFWにResourcesフォルダをコピー
9. [Processing.NDI.Lib.x64.dll]() をx64/ReleseGLFWにコピー
10. VirtualLive-2025-02-Garnet.exeを実行
