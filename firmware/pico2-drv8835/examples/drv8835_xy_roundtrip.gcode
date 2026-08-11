; DRV8835 XY動作確認
; X: 下層配線向け D100、各状態300ms
; Y: 磁石に近い配線向け D100、各状態200ms
; VM 3V、各相1.5ohm 5W以上の直列抵抗、電源電流制限1.5Aを前提とする

; --- X軸を1電気周期前進して戻す ---
M974 X S0 D100 P300 ; X開始 A+
M974 X S1 D100 P300 ; X前進 B-
M974 X S2 D100 P300 ; X前進 A-
M974 X S3 D100 P300 ; X前進 B+
M974 X S0 D100 P300 ; X前進 A+
M974 X S3 D100 P300 ; X後退 B+
M974 X S2 D100 P300 ; X後退 A-
M974 X S1 D100 P300 ; X後退 B-
M974 X S0 D100 P300 ; X開始位置へ戻る A+
M18                  ; 全出力Hi-Z

; --- Y軸を1電気周期前進して戻す ---
M974 Y S0 D100 P200 ; Y開始 C+
M974 Y S1 D100 P200 ; Y前進 D-
M974 Y S2 D100 P200 ; Y前進 C-
M974 Y S3 D100 P200 ; Y前進 D+
M974 Y S0 D100 P200 ; Y前進 C+
M974 Y S3 D100 P200 ; Y後退 D+
M974 Y S2 D100 P200 ; Y後退 C-
M974 Y S1 D100 P200 ; Y後退 D-
M974 Y S0 D100 P200 ; Y開始位置へ戻る C+
M18                ; 全出力Hi-Z

; --- XYを交互に進めて斜め方向へ前進し、同じ経路を戻す ---
M974 X S0 D100 P300 ; XY開始 X A+
M974 Y S0 D100 P200  ; XY開始 Y C+
M974 X S1 D100 P300 ; XY前進 X B-
M974 Y S1 D100 P200  ; XY前進 Y D-
M974 X S2 D100 P300 ; XY前進 X A-
M974 Y S2 D100 P200  ; XY前進 Y C-
M974 X S3 D100 P300 ; XY前進 X B+
M974 Y S3 D100 P200  ; XY前進 Y D+
M974 X S0 D100 P300 ; XY前進 X A+
M974 Y S0 D100 P200  ; XY前進 Y C+
M974 X S3 D100 P300 ; XY後退 X B+
M974 Y S3 D100 P200  ; XY後退 Y D+
M974 X S2 D100 P300 ; XY後退 X A-
M974 Y S2 D100 P200  ; XY後退 Y C-
M974 X S1 D100 P300 ; XY後退 X B-
M974 Y S1 D100 P200  ; XY後退 Y D-
M974 X S0 D100 P300 ; XY開始位置へ戻る X A+
M974 Y S0 D100 P200  ; XY開始位置へ戻る Y C+
M18                  ; 全出力Hi-Z
