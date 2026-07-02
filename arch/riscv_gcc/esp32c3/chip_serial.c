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
 *  arch/arm_m_gcc/rp2350/chip_serial.c からの流用．UART実体をRP2350
 *  UART(PL011系)からESP32-C3 UARTに置換．
 */

#include <kernel.h>
#include <t_syslog.h>
#include "target_syssvc.h"
#include "chip_serial.h"

/*
 *  低レベル出力用のSIOポート管理ブロック
 */
static SIOPCB	*p_siopcb_target_fput;

/*
 *  SIOドライバの初期化
 */
void
sio_initialize(EXINF exinf)
{
	esp32c3_uart_initialize();
}

/*
 *  SIOドライバの終了処理
 */
void
sio_terminate(EXINF exinf)
{
	esp32c3_uart_terminate();
}

/*
 *  SIOの割込みサービスルーチン
 */
void
sio_isr(EXINF exinf)
{
	esp32c3_uart_isr((ID) exinf);
}

/*
 *  SIOポートのオープン
 */
SIOPCB *
sio_opn_por(ID siopid, EXINF exinf)
{
	SIOPCB	*p_siopcb;

	/*
	 *  デバイス依存のオープン処理
	 */
	p_siopcb = esp32c3_uart_opn_por(siopid, exinf);

	/*
	 *  低レベル出力用のSIOポートを記録する．
	 */
	if (siopid == SIOPID_FPUT) {
		p_siopcb_target_fput = p_siopcb;
	}

	/*
	 *  SIOの割込みマスクを解除する．
	 */
	(void) ena_int(INTNO_SIO);
	return(p_siopcb);
}

/*
 *  SIOポートのクローズ
 */
void
sio_cls_por(SIOPCB *p_siopcb)
{
	/*
	 *  デバイス依存のクローズ処理
	 */
	esp32c3_uart_cls_por(p_siopcb);

	/*
	 *  SIOの割込みをマスクする．
	 */
	(void) dis_int(INTNO_SIO);
}

/*
 *  SIOポートへの文字送信
 */
bool_t
sio_snd_chr(SIOPCB *p_siopcb, char c)
{
	return(esp32c3_uart_snd_chr(p_siopcb, c));
}

/*
 *  SIOポートからの文字受信
 */
int_t
sio_rcv_chr(SIOPCB *p_siopcb)
{
	return(esp32c3_uart_rcv_chr(p_siopcb));
}

/*
 *  SIOポートからのコールバックの許可
 */
void
sio_ena_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	esp32c3_uart_ena_cbr(p_siopcb, cbrtn);
}

/*
 *  SIOポートからのコールバックの禁止
 */
void
sio_dis_cbr(SIOPCB *p_siopcb, uint_t cbrtn)
{
	esp32c3_uart_dis_cbr(p_siopcb, cbrtn);
}

/*
 *  SIOポートからの送信可能コールバック
 */
void
esp32c3_uart_irdy_snd(EXINF exinf)
{
	sio_irdy_snd(exinf);
}

/*
 *  SIOポートからの受信通知コールバック
 */
void
esp32c3_uart_irdy_rcv(EXINF exinf)
{
	sio_irdy_rcv(exinf);
}

/*
 *		システムログの低レベル出力
 */

/*
 *  SIOポートへのポーリング出力
 */
static void
esp32c3_uart_fput(char c)
{
	/*
	 *  送信できるまでポーリング
	 */
	while (!(esp32c3_uart_snd_chr(p_siopcb_target_fput, c))) {
		sil_dly_nse(100);
	}
}

/*
 *  SIOポートへの文字出力
 */
void
target_fput_log(char c)
{
	if (c == '\n') {
		esp32c3_uart_fput('\r');
	}
	esp32c3_uart_fput(c);
}
