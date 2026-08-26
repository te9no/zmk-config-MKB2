# Futhesia Moduora (MeKaBu) - フセシア・モデュオラ（メカブ）

[![Build Health](badges/build-health/zmk-config-MKB2/main/build-health.svg)](https://github.com/te9no/zmk-config-MKB2/actions/workflows/build.yml?query=branch%3Amain)

```
                                                                                         
                                :-:                                                      
                               =@@@%=                                                    
                               .#@@@@%-                                                  
                                 =@@@@@=         :%%#:                                   
                    :*#*:         :@@@@@-        *@@@%                                   
                    +@@@@#.        +@@@@#        %@@@%                                   
                     =@@@@@-       -@@@@%       +@@@@*                                   
                      .#@@@@=      -@@@@%     .#@@@@%.                                   
                        #@@@@.     #@@@@=    =@@@@@%:                                    
                        :@@@@=    *@@@@*   -%@@@@@*                                      
                        :@@@@=   *@@@@#   +@@@@@*:   -#%#-                               
                        =@@@@.  =@@@@#   #@@@@#:    -@@@@%                               
                        *@@@#   %@@@@.  *@@@@=    .=@@@@@=                               
                        %@@@*   @@@@*  -@@@@-   :*%@@@@@=                                
                        #@@@#   #@@@=  #@@@=  .#@@@@@#+.                                 
                        =@@@@-  -@@@+  %@@*  :@@@@#=.                                    
         --.             +@@@@:  +@@%  #@@: :@@@#:                .--                    
       .%*=%*=======:     -%@@@=  *@@- *@@  %@@-      -==========*%+*%.                  
       :@*-%#++++++*@#.     -#@@#: #@# =@% +@%:     -%%++++++++++#@-+@:                  
        .==:        .*@+      :+@@= %@:-@*.@%.    .*@=            :==.                   
           .:         :#%=      .+@+-@*=@=#%.    +@*.            :.                      
         .%**%=-----:   -%#---:   +@*%%%@%@:  :=%#:   :--------=%**%:                    
         -@=-@#*****%#:   =***#%+*@@@@@@@@@%+#%*-   -%%********#@--@=                    
          :++-       +@#:      .+@@@@@@@@@@@*-    :#%=          -++-                     
                      .*@#*******%@@@@@@@@@@*****#@+                                     
                        .:::::::::::::::::::::::::.                                      
                                                                                         
```

A distributed knowledge-type input device discovered in Realm of Split IV.  
分割界・第四層で発見された集合知式入力装置群

## Classification | 分類
- **Species**: Synaptica Modularis (集合知式入力装置群)
- **Common Name**: MeKaBu (メカブ)
- **Origin**: Realm of Split IV (分割界・第四層)
- **Distribution**: Personal Workshops & Community Development Zones (個人工房、コミュニティ開発圏)
- **Nature**: Extensible, Autonomous, Harmonious (拡張可能・自律型・協奏性)

## Overview | 概要
Futhesia Moduora is an intellectually and structurally evolving input device born from the collective knowledge and experience of multiple developers. While based on a split-grid input surface, its form can be freely altered through external modules (known as Nodes).

フセシア・モデュオラは、複数の開発者が知識と経験を持ち寄ることによって生まれた、知的・構造的進化を続ける入力装置である。その外見は、左右に分かれた格子状の入力面を基軸としながらも、外部モジュール（通称：Node）によって自在に形を変える。

## Key Features | 特徴
- **Cognitive Lattice (知の格子)**: Key layouts designed at the intersection of functional beauty and logic
- **Modular Nexus (拡張構造)**: Freely interchangeable pointing devices, encoders, and sensors
- **Resonant Evolution (共鳴型進化)**: Organic integration of developer codes and designs
- **Tectonic Mode (テンティング適応)**: Physically adaptable form-factor

## Natural Habitat | 生態／運用環境
Thrives in collaborative environments, particularly hackathons and technical conventions.
単体での生息よりも、共創型の環境下において最も高いパフォーマンスを発揮する。

### Current Keymap Configuration | 現在のキーマップ構成
![Keymap](keymap-svg/MKB.svg)

### Pipette keymap editing | Pipetteキーマップ編集

The left/central firmware exposes a VIA/Vial Raw HID interface for
[Pipette](https://github.com/darakuneko/pipette-desktop). The keyboard definition is stored in
[`config/vial.json`](config/vial.json) and embedded into the firmware during the build.

左側（Central）のファームウェアは、Pipette用のVIA/Vial Raw HIDインターフェースを公開します。
左側をUSB接続してPipetteを開くと、基本的なキーコードを編集できます。変更はZMK settingsへ
保存されます。

Current limitations / 現在の制限:

- `&kp`, `&none`, and `&trans` are editable.
- Hold-tap, layer-tap, mouse, Bluetooth, and other ZMK-specific bindings appear as `KC_NO`.
  Do not overwrite those cells unless replacing them with a basic key.
- Macros, encoders, lighting, and Vial dynamic entries are not exposed through Pipette yet.
- The current Vial unlock mode is intentionally insecure and intended for local firmware use.

## Etymology | 語源
The name "MeKaBu" encompasses multiple meanings:
- Mechanical Components (メカニカルな部品群)
- Sprouting Stock - branching growth structure (芽株 - 分岐して増殖する構造)

---
*This configuration exists in the liminal space between reality and digital dreams.*  
*この設定は、現実とデジタルの夢の間の境界に存在する。*
