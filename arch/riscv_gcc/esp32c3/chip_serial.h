/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，本ソフトウェアを TOPPERS ライセンス（条件は他のソー
 *  スファイルの先頭コメントを参照）の下で利用することを許諾する．本ソフ
 *  トウェアは無保証で提供される．
 *
 */

/*
 *		シリアルインタフェースドライバのチップ依存部（ESP32-C3用）
 *		（非TECS版専用）
 *
 *  arch/arm_m_gcc/rp2350/chip_serial.h からの流用．UART実体をRP2350
 *  UART(PL011系)からESP32-C3 UARTに置換．
 */

#ifndef TOPPERS_CHIP_SERIAL_H
#define TOPPERS_CHIP_SERIAL_H

#include "esp32c3_uart.h"

#ifndef TOPPERS_MACRO_ONLY

/*
 *  SIOドライバの初期化
 */
extern void sio_initialize(EXINF exinf);

/*
 *  SIOドライバの終了処理
 */
extern void sio_terminate(EXINF exinf);

/*
 *  SIOの割込みサービスルーチン
 */
extern void sio_isr(EXINF exinf);

/*
 *  SIOポートのオープン
 */
extern SIOPCB *sio_opn_por(ID siopid, EXINF exinf);

/*
 *  SIOポートのクローズ
 */
extern void sio_cls_por(SIOPCB *p_siopcb);

/*
 *  SIOポートへの文字送信
 */
extern bool_t sio_snd_chr(SIOPCB *p_siopcb, char c);

/*
 *  SIOポートからの文字受信
 */
extern int_t sio_rcv_chr(SIOPCB *p_siopcb);

/*
 *  SIOポートからのコールバックの許可
 */
extern void sio_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn);

/*
 *  SIOポートからのコールバックの禁止
 */
extern void sio_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn);

/*
 *  SIOポートからの送信可能コールバック
 */
extern void sio_irdy_snd(EXINF exinf);

/*
 *  SIOポートからの受信通知コールバック
 */
extern void sio_irdy_rcv(EXINF exinf);

#endif /* TOPPERS_MACRO_ONLY */
#endif /* TOPPERS_CHIP_SERIAL_H */
