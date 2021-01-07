//
// 2020年7月 電子工作実習でコーディングした室内温度計（Arduino内部のCPUの温度計を使用）
// <参考> http://radiopench.blog96.fc2.com/blog-entry-489.html <最終アクセス：20200604>
// <参考> https://avr.jp/user/DS/PDF/mega88A.pdf <最終アクセス：20200604>
// <参考> https://avr.jp/user/DS/PDF/megaXX8PA.pdf <最終アクセス：20200618>
// <参考> http://ww1.microchip.com/downloads/en/Appnotes/Atmel-8108-Calibration-of-the-AVRs-Internal-Temperature-Reference_ApplicationNote_AVR122.pdf <最終アクセス：20200618>
// <参考> https://www.clarestudio.org/elec/avr/mcu-clock.html <最終アクセス：20200702>
//
// AD変換エラーを抑えるために，システムクロックを低速（500kHz）にする。
// 温度感知器に接続されたADCからデータを読み出してセルシウス温度に変換し，シリアルモニタに表示する。
// シリアルモニタから現在の温度を入力することで，温度を校正できる。
//


long sumd;
float temp;
float offset_temp;
float cal_temp;
int len_input;
byte val;


void setup() {
  noInterrupts();                                   // クロック設定の最中は割り込み禁止(データシートpage24)。
  CLKPR = ( 1 << CLKPCE );                          // PLKPCEビットを立てる。詳細はsetup()関数の下のコメントで示す。
  CLKPR = 100;                                      // 分周比を16に設定。8MHzクロックを16分周だから，クロック周波数は500kHz。
  interrupts();                                     // 設定が終わったら割り込み許可。

  Serial.begin(38400);                              // 38400/16=2400より，シリアルモニタのボーレートは2400 bpsに設定する。
  
  offset_temp = 342.5;
}
                                                    //  // 以下のコードを実行した結果，offset_tempは342.5と判明した。
                                                    //  sumd = 0;
                                                    //  adcSetup(0xC8);
                                                    //  for (int n=0; n<1000; n++){sumd += adc();}
                                                    //  temp = 27.0;
                                                    //  offset_temp = (sumd * 1.1 / 1024.0) - temp;
                                                    //  Serial.print("offset_temp == ");
                                                    //  Serial.println(offset_temp);


//      *** CLKPR (クロック前置分周レジスタ)
//         CLKPR = CLKPCE | - | - | - | CLKPS3 | CLKPS2 | CLKPS1 | CLKPS0
//      
//      ** CLKPCE (Clock Prescaler Change Enable)
//        分周値選択手順(データシートpage24)
//          1.CLKPCEに1を, 全CLKPSに0を書き込む。 CLKPR = 1000 0000 → CLKPR = ( 1 << CLKPCE ) ※1を7だけ左ビットシフト
//          2.すぐさまCLKPCEに0を, CLKPSに希望する分周比を書き込む 。 CLKPR = 100 ※16進数ではなく，接頭語無しの2進数で入力。
//      
//      ** CLKPS<3:0> (Clock Prescaler Select Bits 3～0)
//          0000 = 1
//          0001 = 2
//          0010 = 4
//          0011 = 8
//          0100 = 16
//          0101 = 32
//          ...
//          0111 = 128
//          1000 = 256
//          1001~1111 = reserved
//      
//          CLKPSの初期値は0011（ヒューズビットCKDIV8=0の時。ヒューズビットCKDIV8=1の時は，CLKPSの初期値は0000）

// ヒューズバイト（データシートpage185）...ヒューズバイトは専用のライタで書き込まないと変更できない。
//今まで一度も書き込んでいないのであれば，工場出荷時の値が代入されている（以下に示す）。
//
// *** LOW (ヒューズ下位バイト)
//    LOW = CKDIV8 | CKOUT | SUT<1:0> | CKSEL<3:0> = 0110 0010 = 0x6
//
// ** CKDIV8=0 (システムクロック8分周選択)(page24,システムクロック前置分周器)
// ** CKOUT=1 (システムクロック出力許可)
// ** SUT<1:0>=10 (起動時間選択)
// ** CKSEL<3:0>=0010 クロック種別選択(0010は8MHz校正付き内蔵RC発振器)



void loop() {
  int num_point = 0;                              // シリアルモニタからの入力文字列に含まれる'.'（小数点）の数

  sumd = 0;
  adcSetup(0xC8);
  for (int n = 0; n < 1000; n++) {sumd += adc();}              
  temp = (sumd * 1.1 / 1024.0) - offset_temp;       
                                                  // adc()関数より得たデジタル電圧バイト列をセルシウス温度に変換する。 
                                                  // まず，adc()[bit] * (1.1/1024)[V/bit]で，デジタル電圧バイト列を温度感知器の出力アナログ電圧に変換する。
                                                  // 温度感知器の特性上，1mV/℃だから，1[V] : 1000[℃]の比が成り立つ。
                                                  // よって，温度感知器出力電圧が adc()*(1.1/1024)[V] の時， 
                                                  // 温度は                    adc()*(1.1/1024) * 1000 = ( adc()*1000 ) * (1.1/1024)[℃]。
                                                  // ( adc()*1000 ) = v1+v2+v3+...+v1000。よって，1000回adc()を実行して，その和sumdを求めれば，           
                                                  // temp = (sumd * 1.1 / 1024.0)。
                                                  // その後オフセットで補正してセルシウス温度に変換する。 
  Serial.println(temp);
  

  if (Serial.available() > 1) {                   // シリアルモニタから現在の温度の入力があった場合，それに合わせてoffset_tempを再設定する。
    len_input = Serial.available() - 1;           // 文字列終端のヌル文字分を引いた値が入力文字数len_inputになる
    cal_temp = 0;
    num_point = 0;

    while (Serial.available() > 1) {
      val = Serial.read();                        // 入力されるのは数字ではなく文字コード。
      val = val - 0x30;                           // 入力された文字コードから，'0'の文字コード0x30を引けば数字に変換される
      if(val == 254){                             // 254=0xFEは'.'（小数点）の文字コード。
        num_point++;
        continue;
      }
      
      cal_temp += val * pow(10.0, (Serial.available()+num_point) - (len_input-1) );              // （例）27.30度 -> 2*(10^1) + 7*(10^0) + 3*(10^-1) + 4*(10^-2)
    }                                             // Serial.available()はSerial.read()が実効される度に1づつ減っていくため，バッファの残り文字数（ヌル文字含む）を表す。
    val = Serial.read();                          // ヌル文字を読み出してバッファを空にする
    
    Serial.print(cal_temp);
    Serial.println("度で校正しました\n");
    sumd = 0;
    adcSetup(0xC8);
    for (int n = 0; n < 1000; n++){sumd += adc();}
    offset_temp = (sumd * 1.1 / 1024.0) - cal_temp;    // temp = (sumd * 1.1 / 1024.0) - offset_tempを式変形すれば，この式が導ける。
  }
}




unsigned int adc() {                     // ADCの出力（バイト列）を返す
  ADCSRA |= ( 1 << ADSC );               // AD変換開始。詳細はadcSetup()関数下のコメント。
  while (ADCSRA & (1 << ADSC) ) {}       // 変換完了待ち
  return ADCL | (ADCH << 8);             // 10ビットに合成した値を返す
}


void adcSetup(byte admux) {               // ADCの設定
  ADMUX = admux;                          // ADCの参照電圧として内部1.1V基準電圧を使用し，温度感知器(ADC8)を選択する。ADMUX == 1100 1000
  ADCSRA |= ( 1 << ADEN );                // ADENのビットを立てて，ADCの電源を入れる。
  ADCSRA |= 0x07;                         // より低速の方がAD変換エラーが少ないため，ADP(AD変換クロック)をCK/128に設定。ADSRA == 1000 0111
}
  
//            datasheet<https://avr.jp/user/DS/PDF/megaXX8PA.pdf> page158~
//          
//            *** ADMUX (ADC Multiplexer Select Register)
//              ADMUX == REFS1 | REFS0 | ADLAR | - | MUX3 | MUX2 | MUX1 | MUX0
//          
//            ** REFS1, REFS0 (AREFピンの電圧の設定，ADコンバータの基準電圧の選択)
//              0b00 外部基準電圧
//              0b01 AVcc（AREFピンと内部基準電圧を切り離し，AREFにバイパスコンデンサを接続）
//              0b10 予約済み
//              0b11 内部 1.1V 基準電圧 <-(これを選択)
//          
//            ** ADLAR
//              Arduino UNOでは，ADLAR==0 （右詰めに設定）
//          
//            ** MUX3,2,1,0（ADコンバータの選択）
//              0b0000 A0
//              0b0001 A1
//              0b0010 A2
//              0b0011 A3
//              0b0100 A4
//              0b0101 A5
//              0b1000 温度 <-(これを選択)
//              0b1001~1101 予約済み
//              0b1110 内部 1.1V 基準電圧
//              0b1111 GND
//          
//          
//            *** ADCSRA (ADC Control and Status Register A)
//              ADCSRA == ADEN | ADSC | ADATE | ADIF | ADIE | ADP2 | ADP1 | ADP0
//          
//            ** ADEN（ADコンバータの電源）
//              1でON，0でOFF
//          
//            ** ADSC（AD変換開始）
//              1で変換開始，変換が完了すると0になる
//          
//            ** ADATE（AD変換の自動起動許可）
//              1で許可，0で禁止
//          
//            ** ADIF（AD変換完了割り込み要求フラグ）
//              AD変換完了後，ADデータレジスタが更新されると1になる
//          
//            ** ADIE（AD変換完了割り込み許可）
//              1で許可，0で禁止
//          
//            ** ADP2,1,0（AD変換クロックの選択）
//              システムクロックの分周して，AD変換クロックとする。その分周比の設定を行う。
//                ADP2 | ADP1 | ADP0
//                000 CK/2
//                001 CK/2
//                010 CK/4
//                011 CK/8
//                100 CK/16
//                101 CK/32
//                110 CK/64
//                111 CK/128 <-(これを選択)
