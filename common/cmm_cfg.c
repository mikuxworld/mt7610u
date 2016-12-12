/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************/


#include "rt_config.h"
#ifdef DOT11_N_SUPPORT
#ifdef RT65xx
#define MAX_AGG_CNT	32
#elif defined(RT2883) || defined(RT3883)
#define MAX_AGG_CNT	16
#else
#define MAX_AGG_CNT	8
#endif
/* DisplayTxAgg - display Aggregation statistics from MAC */
void DisplayTxAgg (struct rtmp_adapter*pAd)
{
	ULONG totalCount;
	ULONG aggCnt[MAX_AGG_CNT + 2];
	int i;

	AsicReadAggCnt(pAd, aggCnt, sizeof(aggCnt) / sizeof(ULONG));
	totalCount = aggCnt[0] + aggCnt[1];
	if (totalCount > 0)
		for (i=0; i<MAX_AGG_CNT; i++) {
			DBGPRINT(RT_DEBUG_OFF, ("\t%d MPDU=%ld (%ld%%)\n", i+1, aggCnt[i+2], aggCnt[i+2]*100/totalCount));
		}
	printk("====================\n");

}
#endif /* DOT11_N_SUPPORT */

static bool RT_isLegalCmdBeforeInfUp(
       IN char *SetCmd);


INT ComputeChecksum(
	IN UINT PIN)
{
	INT digit_s;
    UINT accum = 0;

	PIN *= 10;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);

	digit_s = (accum % 10);
	return ((10 - digit_s) % 10);
} /* ComputeChecksum*/

UINT GenerateWpsPinCode(
	IN	struct rtmp_adapter *pAd,
    IN  bool         bFromApcli,
	IN	u8 		apidx)
{
	u8 macAddr[ETH_ALEN];
	UINT 	iPin;
	UINT	checksum;

	memset(macAddr, 0, ETH_ALEN);

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		memmove(&macAddr[0], pAd->CurrentAddress, ETH_ALEN);
#endif /* CONFIG_STA_SUPPORT */

	iPin = macAddr[3] * 256 * 256 + macAddr[4] * 256 + macAddr[5];

	iPin = iPin % 10000000;


	checksum = ComputeChecksum( iPin );
	iPin = iPin*10 + checksum;

	return iPin;
}


static char *phy_mode_str[]={"CCK", "OFDM", "HTMIX", "GF", "VHT"};
char* get_phymode_str(int Mode)
{
	if (Mode >= MODE_CCK && Mode <= MODE_VHT)
		return phy_mode_str[Mode];
	else
		return "N/A";
}


static u8 *phy_bw_str[] = {"20M", "40M", "80M", "10M"};
char* get_bw_str(int bandwidth)
{
	if (bandwidth >= BW_20 && bandwidth <= BW_10)
		return phy_bw_str[bandwidth];
	else
		return "N/A";
}


/*
    ==========================================================================
    Description:
        Set Country Region to pAd->CommonCfg.CountryRegion.
        This command will not work, if the field of CountryRegion in eeprom is programmed.

    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
*/
INT RT_CfgSetCountryRegion(
	IN struct rtmp_adapter *pAd,
	IN char *		arg,
	IN INT				band)
{
	LONG region;
	u8 *pCountryRegion;

	region = simple_strtol(arg, 0, 10);

	if (band == BAND_24G)
		pCountryRegion = &pAd->CommonCfg.CountryRegion;
	else
		pCountryRegion = &pAd->CommonCfg.CountryRegionForABand;

    /*
               1. If this value is set before interface up, do not reject this value.
               2. Country can be set only when EEPROM not programmed
    */
    if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE) && (*pCountryRegion & EEPROM_IS_PROGRAMMED))
	{
		DBGPRINT(RT_DEBUG_ERROR, ("CfgSetCountryRegion():CountryRegion in eeprom was programmed\n"));
		return false;
	}

	if((region >= 0) &&
	   (((band == BAND_24G) &&((region <= REGION_MAXIMUM_BG_BAND) ||
	   (region == REGION_31_BG_BAND) || (region == REGION_32_BG_BAND) || (region == REGION_33_BG_BAND) )) ||
	    ((band == BAND_5G) && (region <= REGION_MAXIMUM_A_BAND) ))
	  )
	{
		*pCountryRegion= (u8) region;
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, ("CfgSetCountryRegion():region(%ld) out of range!\n", region));
		return false;
	}

	return true;

}


static u8 CFG_WMODE_MAP[]={
	PHY_11BG_MIXED, (WMODE_B | WMODE_G), /* 0 => B/G mixed */
	PHY_11B, (WMODE_B), /* 1 => B only */
	PHY_11A, (WMODE_A), /* 2 => A only */
	PHY_11ABG_MIXED, (WMODE_A | WMODE_B | WMODE_G), /* 3 => A/B/G mixed */
	PHY_11G, WMODE_G, /* 4 => G only */
	PHY_11ABGN_MIXED, (WMODE_B | WMODE_G | WMODE_GN | WMODE_A | WMODE_AN), /* 5 => A/B/G/GN/AN mixed */
	PHY_11N_2_4G, (WMODE_GN), /* 6 => N in 2.4G band only */
	PHY_11GN_MIXED, (WMODE_G | WMODE_GN), /* 7 => G/GN, i.e., no CCK mode */
	PHY_11AN_MIXED, (WMODE_A | WMODE_AN), /* 8 => A/N in 5 band */
	PHY_11BGN_MIXED, (WMODE_B | WMODE_G | WMODE_GN), /* 9 => B/G/GN mode*/
	PHY_11AGN_MIXED, (WMODE_G | WMODE_GN | WMODE_A | WMODE_AN), /* 10 => A/AN/G/GN mode, not support B mode */
	PHY_11N_5G, (WMODE_AN), /* 11 => only N in 5G band */
#ifdef DOT11_VHT_AC
	PHY_11VHT_N_ABG_MIXED, (WMODE_B | WMODE_G | WMODE_GN |WMODE_A | WMODE_AN | WMODE_AC), /* 12 => B/G/GN/A/AN/AC mixed*/
	PHY_11VHT_N_AG_MIXED, (WMODE_G | WMODE_GN |WMODE_A | WMODE_AN | WMODE_AC), /* 13 => G/GN/A/AN/AC mixed , no B mode */
	PHY_11VHT_N_A_MIXED, (WMODE_A | WMODE_AN | WMODE_AC), /* 14 => A/AC/AN mixed */
	PHY_11VHT_N_MIXED, (WMODE_AN | WMODE_AC), /* 15 => AC/AN mixed, but no A mode */
#endif /* DOT11_VHT_AC */
	PHY_MODE_MAX, WMODE_INVALID /* default phy mode if not match */
};


static char *BAND_STR[] = {"Invalid", "2.4G", "5G", "2.4G/5G"};
static char *WMODE_STR[]= {"", "A", "B", "G", "gN", "aN", "AC"};

u8 *wmode_2_str(u8 wmode)
{
	u8 *str;
	INT idx, pos, max_len;

	max_len = WMODE_COMP * 3;
	str = kmalloc(max_len, GFP_ATOMIC);
	if (str) {
		memset(str, 0, max_len);
		pos = 0;
		for (idx = 0; idx < WMODE_COMP; idx++)
		{
			if (wmode & (1 << idx)) {
				if ((strlen(str) +  strlen(WMODE_STR[idx + 1])) >= (max_len - 1))
					break;
				if (strlen(str)) {
					memmove(&str[pos], "/", 1);
					pos++;
				}
				memmove(&str[pos], WMODE_STR[idx + 1], strlen(WMODE_STR[idx + 1]));
				pos += strlen(WMODE_STR[idx + 1]);
			}
			if (strlen(str) >= max_len)
				break;
		}

		return str;
	}
	else
		return NULL;
}


u8 cfgmode_2_wmode(u8 cfg_mode)
{
	DBGPRINT(RT_DEBUG_OFF, ("cfg_mode=%d\n", cfg_mode));
	if (cfg_mode >= PHY_MODE_MAX)
		cfg_mode =  PHY_MODE_MAX;

	return CFG_WMODE_MAP[cfg_mode * 2 + 1];
}


static bool wmode_valid(struct rtmp_adapter*pAd, enum WIFI_MODE wmode)
{
	if ((WMODE_CAP_5G(wmode) && (!PHY_CAP_5G(pAd->chipCap.phy_caps))) ||
		(WMODE_CAP_2G(wmode) && (!PHY_CAP_2G(pAd->chipCap.phy_caps))) ||
		(WMODE_CAP_N(wmode) && RTMP_TEST_MORE_FLAG(pAd, fRTMP_ADAPTER_DISABLE_DOT_11N))
	)
		return false;
	else
		return true;
}


bool wmode_band_equal(u8 smode, u8 tmode)
{
	bool eq = false;
	u8 *str1, *str2;

	if ((WMODE_CAP_5G(smode) == WMODE_CAP_5G(tmode)) &&
		(WMODE_CAP_2G(smode) == WMODE_CAP_2G(tmode)))
		eq = true;

	str1 = wmode_2_str(smode);
	str2 = wmode_2_str(tmode);
	if (str1 && str2)
	{
		DBGPRINT(RT_DEBUG_TRACE,
			("Old WirelessMode:%s(0x%x), "
			 "New WirelessMode:%s(0x%x)!\n",
			str1, smode, str2, tmode));
	}
	if (str1)
		kfree(str1);
	if (str2)
		kfree(str2);

	return eq;
}


/*
    ==========================================================================
    Description:
        Set Wireless Mode
    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
*/
INT RT_CfgSetWirelessMode(struct rtmp_adapter*pAd, char *arg)
{
	LONG cfg_mode;
	u8 wmode, *mode_str;


	cfg_mode = simple_strtol(arg, 0, 10);

	/* check if chip support 5G band when WirelessMode is 5G band */
	wmode = cfgmode_2_wmode((u8)cfg_mode);
	if ((wmode == WMODE_INVALID) || (!wmode_valid(pAd, wmode))) {
		DBGPRINT(RT_DEBUG_ERROR,
				("%s(): Invalid wireless mode(%ld, wmode=0x%x), ChipCap(%s)\n",
				__FUNCTION__, cfg_mode, wmode,
				BAND_STR[pAd->chipCap.phy_caps & 0x3]));
		return false;
	}

	if (wmode_band_equal(pAd->CommonCfg.PhyMode, wmode) == true)
		DBGPRINT(RT_DEBUG_OFF, ("wmode_band_equal(): Band Equal!\n"));
	else
		DBGPRINT(RT_DEBUG_OFF, ("wmode_band_equal(): Band Not Equal!\n"));

	pAd->CommonCfg.PhyMode = wmode;
	pAd->CommonCfg.cfg_wmode = wmode;

	mode_str = wmode_2_str(wmode);
	if (mode_str)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("%s(): Set WMODE=%s(0x%x)\n",
				__FUNCTION__, mode_str, wmode));
		kfree(mode_str);
	}

	return true;
}


/* maybe can be moved to GPL code, ap_mbss.c, but the code will be open */


static bool RT_isLegalCmdBeforeInfUp(
       IN char *SetCmd)
{
		bool TestFlag;
		TestFlag =	!strcmp(SetCmd, "Debug") ||
#ifdef EXT_BUILD_CHANNEL_LIST
					!strcmp(SetCmd, "CountryCode") ||
					!strcmp(SetCmd, "DfsType") ||
					!strcmp(SetCmd, "ChannelListAdd") ||
					!strcmp(SetCmd, "ChannelListShow") ||
					!strcmp(SetCmd, "ChannelListDel") ||
#endif /* EXT_BUILD_CHANNEL_LIST */
					false; /* default */
       return TestFlag;
}


INT RT_CfgSetShortSlot(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	LONG ShortSlot;

	ShortSlot = simple_strtol(arg, 0, 10);

	if (ShortSlot == 1)
		pAd->CommonCfg.bUseShortSlotTime = true;
	else if (ShortSlot == 0)
		pAd->CommonCfg.bUseShortSlotTime = false;
	else
		return false;  /*Invalid argument */

	return true;
}


/*
    ==========================================================================
    Description:
        Set WEP KEY base on KeyIdx
    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
*/
INT	RT_CfgSetWepKey(
	IN	struct rtmp_adapter *pAd,
	IN	char *		keyString,
	IN	CIPHER_KEY		*pSharedKey,
	IN	INT				keyIdx)
{
	INT				KeyLen;
	INT				i;
	/*u8 		CipherAlg = CIPHER_NONE;*/
	bool 		bKeyIsHex = false;

	/* TODO: Shall we do memset for the original key info??*/
	memset(pSharedKey, 0, sizeof(CIPHER_KEY));
	KeyLen = strlen(keyString);
	switch (KeyLen)
	{
		case 5: /*wep 40 Ascii type*/
		case 13: /*wep 104 Ascii type*/
			bKeyIsHex = false;
			pSharedKey->KeyLen = KeyLen;
			memmove(pSharedKey->Key, keyString, KeyLen);
			break;

		case 10: /*wep 40 Hex type*/
		case 26: /*wep 104 Hex type*/
			for(i=0; i < KeyLen; i++)
			{
				if( !isxdigit(*(keyString+i)) )
					return false;  /*Not Hex value;*/
			}
			bKeyIsHex = true;
			pSharedKey->KeyLen = KeyLen/2 ;
			AtoH(keyString, pSharedKey->Key, pSharedKey->KeyLen);
			break;

		default: /*Invalid argument */
			DBGPRINT(RT_DEBUG_TRACE, ("RT_CfgSetWepKey(keyIdx=%d):Invalid argument (arg=%s)\n", keyIdx, keyString));
			return false;
	}

	pSharedKey->CipherAlg = ((KeyLen % 5) ? CIPHER_WEP128 : CIPHER_WEP64);
	DBGPRINT(RT_DEBUG_TRACE, ("RT_CfgSetWepKey:(KeyIdx=%d,type=%s, Alg=%s)\n",
						keyIdx, (bKeyIsHex == false ? "Ascii" : "Hex"), CipherName[pSharedKey->CipherAlg]));

	return true;
}


/*
    ==========================================================================
    Description:
        Set WPA PSK key

    Arguments:
        pAdapter	Pointer to our adapter
        keyString	WPA pre-shared key string
        pHashStr	String used for password hash function
        hashStrLen	Lenght of the hash string
        pPMKBuf		Output buffer of WPAPSK key

    Return:
        true if all parameters are OK, false otherwise
    ==========================================================================
*/
INT RT_CfgSetWPAPSKKey(
	IN struct rtmp_adapter*pAd,
	IN char *	keyString,
	IN INT			keyStringLen,
	IN u8 	*pHashStr,
	IN INT			hashStrLen,
	OUT u8 *	pPMKBuf)
{
	u8 keyMaterial[40];

	if ((keyStringLen < 8) || (keyStringLen > 64))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("WPAPSK Key length(%d) error, required 8 ~ 64 characters!(keyStr=%s)\n",
									keyStringLen, keyString));
		return false;
	}

	memset(pPMKBuf, 0, 32);
	if (keyStringLen == 64)
	{
	    AtoH(keyString, pPMKBuf, 32);
	}
	else
	{
	    RtmpPasswordHash(keyString, pHashStr, hashStrLen, keyMaterial);
	    memmove(pPMKBuf, keyMaterial, 32);
	}

	return true;
}

INT	RT_CfgSetFixedTxPhyMode(char *arg)
{
	INT fix_tx_mode = FIXED_TXMODE_HT;
	ULONG value;


	if (rtstrcasecmp(arg, "OFDM") == true)
		fix_tx_mode = FIXED_TXMODE_OFDM;
	else if (rtstrcasecmp(arg, "CCK") == true)
	    fix_tx_mode = FIXED_TXMODE_CCK;
	else if (rtstrcasecmp(arg, "HT") == true)
	    fix_tx_mode = FIXED_TXMODE_HT;
	else if (rtstrcasecmp(arg, "VHT") == true)
		fix_tx_mode = FIXED_TXMODE_VHT;
	else
	{
		value = simple_strtol(arg, 0, 10);
		switch (value)
		{
			case FIXED_TXMODE_CCK:
			case FIXED_TXMODE_OFDM:
			case FIXED_TXMODE_HT:
			case FIXED_TXMODE_VHT:
				fix_tx_mode = value;
			default:
				fix_tx_mode = FIXED_TXMODE_HT;
		}
	}

	return fix_tx_mode;

}

INT	RT_CfgSetMacAddress(
	IN 	struct rtmp_adapter *	pAd,
	IN	char *		arg)
{
	INT	i, mac_len;

	/* Mac address acceptable format 01:02:03:04:05:06 length 17 */
	mac_len = strlen(arg);
	if(mac_len != 17)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s : invalid length (%d)\n", __FUNCTION__, mac_len));
		return false;
	}

	if(strcmp(arg, "00:00:00:00:00:00") == 0)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("%s : invalid mac setting \n", __FUNCTION__));
		return false;
	}

	for (i = 0; i < ETH_ALEN; i++)
	{
		AtoH(arg, &pAd->CurrentAddress[i], 1);
		arg = arg + 3;
	}

	pAd->bLocalAdminMAC = true;
	return true;
}

INT	RT_CfgSetTxMCSProc(char *arg, bool *pAutoRate)
{
	INT	Value = simple_strtol(arg, 0, 10);
	INT	TxMcs;

	if ((Value >= 0 && Value <= 23) || (Value == 32)) /* 3*3*/
	{
		TxMcs = Value;
		*pAutoRate = false;
	}
	else
	{
		TxMcs = MCS_AUTO;
		*pAutoRate = true;
	}

	return TxMcs;

}

INT	RT_CfgSetAutoFallBack(
	IN 	struct rtmp_adapter *	pAd,
	IN	char *		arg)
{
	TX_RTY_CFG_STRUC tx_rty_cfg;
	u8 AutoFallBack = (u8)simple_strtol(arg, 0, 10);

	mt7610u_read32(pAd, TX_RTY_CFG, &tx_rty_cfg.word);
	tx_rty_cfg.field.TxautoFBEnable = (AutoFallBack) ? 1 : 0;
	mt7610u_write32(pAd, TX_RTY_CFG, tx_rty_cfg.word);
	DBGPRINT(RT_DEBUG_TRACE, ("RT_CfgSetAutoFallBack::(tx_rty_cfg=0x%x)\n", tx_rty_cfg.word));
	return true;
}


/*
========================================================================
Routine Description:
	Handler for CMD_RTPRIV_IOCTL_STA_SIOCGIWNAME.

Arguments:
	pAd				- WLAN control block pointer
	*pData			- the communication data pointer
	Data			- the communication data

Return Value:
	NDIS_STATUS_SUCCESS or NDIS_STATUS_FAILURE

Note:
========================================================================
*/
INT RtmpIoctl_rt_ioctl_giwname(
	IN	struct rtmp_adapter		*pAd,
	IN	void 				*pData,
	IN	ULONG					Data)
{
	u8 CurOpMode = OPMODE_AP;

	if (CurOpMode == OPMODE_AP)
	{
		strcpy(pData, "RTWIFI SoftAP");
	}

	return NDIS_STATUS_SUCCESS;
}


INT RTMP_COM_IoctlHandle(
	IN	struct rtmp_adapter *pAd,
	IN	RTMP_IOCTL_INPUT_STRUCT	*wrq,
	IN	INT						cmd,
	IN	USHORT					subcmd,
	IN	void 				*pData,
	IN	ULONG					Data)
{
	struct os_cookie *pObj = pAd->OS_Cookie;
	INT Status = NDIS_STATUS_SUCCESS, i;
	u8 PermanentAddress[ETH_ALEN];
	USHORT Addr01, Addr23, Addr45;


	pObj = pObj; /* avoid compile warning */

	switch(cmd)
	{
		case CMD_RTPRIV_IOCTL_NETDEV_GET:
		/* get main net_dev */
		{
			void **ppNetDev = (void **)pData;
			*ppNetDev = (void *)(pAd->net_dev);
		}
			break;

		case CMD_RTPRIV_IOCTL_NETDEV_SET:
		/* set main net_dev */
			pAd->net_dev = pData;

			break;

		case CMD_RTPRIV_IOCTL_OPMODE_GET:
		/* get Operation Mode */
			*(ULONG *)pData = pAd->OpMode;
			break;


		case CMD_RTPRIV_IOCTL_TASK_LIST_GET:
		/* get all Tasks */
		{
			RT_CMD_WAIT_QUEUE_LIST *pList = (RT_CMD_WAIT_QUEUE_LIST *)pData;

			pList->pMlmeTask = &pAd->mlmeTask;
#ifdef RTMP_TIMER_TASK_SUPPORT
			pList->pTimerTask = &pAd->timerTask;
#endif /* RTMP_TIMER_TASK_SUPPORT */
			pList->pCmdQTask = &pAd->cmdQTask;
		}
			break;

		case CMD_RTPRIV_IOCTL_IRQ_INIT:
		/* init IRQ */
			RTMP_IRQ_INIT(pAd);
			break;

		case CMD_RTPRIV_IOCTL_IRQ_RELEASE:
		/* release IRQ */
			RTMP_OS_IRQ_RELEASE(pAd, pAd->net_dev);
			break;


		case CMD_RTPRIV_IOCTL_NIC_NOT_EXIST:
		/* set driver state to fRTMP_ADAPTER_NIC_NOT_EXIST */
			RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST);
			break;

		case CMD_RTPRIV_IOCTL_MCU_SLEEP_CLEAR:
			RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_MCU_SLEEP);
			break;

#ifdef CONFIG_STA_SUPPORT
//#ifdef CONFIG_PM
#ifdef RTMP_USB_SUPPORT
		case CMD_RTPRIV_IOCTL_ADAPTER_SUSPEND_SET:
		/* set driver state to fRTMP_ADAPTER_SUSPEND */
			RTMP_SET_FLAG(pAd,fRTMP_ADAPTER_SUSPEND);
			break;

		case CMD_RTPRIV_IOCTL_ADAPTER_SUSPEND_CLEAR:
		/* clear driver state to fRTMP_ADAPTER_SUSPEND */
			RTMP_CLEAR_FLAG(pAd,fRTMP_ADAPTER_SUSPEND);
			RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MCU_SEND_IN_BAND_CMD);
			RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_MCU_SLEEP);
			break;

		case CMD_RTPRIV_IOCTL_ADAPTER_SEND_DISSASSOCIATE:
		/* clear driver state to fRTMP_ADAPTER_SUSPEND */
			if (INFRA_ON(pAd) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
			{
				MLME_DISASSOC_REQ_STRUCT	DisReq;
				MLME_QUEUE_ELEM *MsgElem;/* = (MLME_QUEUE_ELEM *) kmalloc(sizeof(MLME_QUEUE_ELEM), MEM_ALLOC_FLAG);*/
				MsgElem = kmalloc(sizeof(MLME_QUEUE_ELEM), GFP_ATOMIC);
				if (MsgElem)
				{
					memcpy(DisReq.Addr, pAd->CommonCfg.Bssid, ETH_ALEN);
					DisReq.Reason =  REASON_DEAUTH_STA_LEAVING;
					MsgElem->Machine = ASSOC_STATE_MACHINE;
					MsgElem->MsgType = MT2_MLME_DISASSOC_REQ;
					MsgElem->MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
					memmove(MsgElem->Msg, &DisReq, sizeof(MLME_DISASSOC_REQ_STRUCT));
					/* Prevent to connect AP again in STAMlmePeriodicExec*/
					pAd->MlmeAux.AutoReconnectSsidLen= 32;
					memset(pAd->MlmeAux.AutoReconnectSsid, 0, pAd->MlmeAux.AutoReconnectSsidLen);
					pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
					MlmeDisassocReqAction(pAd, MsgElem);/*				kfree(MsgElem);*/
					kfree(MsgElem);
				}
				/*				RTMPusecDelay(1000);*/
				RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CGIWAP, -1, NULL, NULL, 0);
			}
			break;

		case CMD_RTPRIV_IOCTL_ADAPTER_SUSPEND_TEST:
		/* test driver state to fRTMP_ADAPTER_SUSPEND */
			//*(u8 *)pData = RTMP_TEST_FLAG(pAd,fRTMP_ADAPTER_SUSPEND);
			break;

		case CMD_RTPRIV_IOCTL_ADAPTER_IDLE_RADIO_OFF_TEST:
		/* test driver state to fRTMP_ADAPTER_IDLE_RADIO_OFF */
			*(u8 *)pData = RTMP_TEST_FLAG(pAd,fRTMP_ADAPTER_IDLE_RADIO_OFF);
			break;

		case CMD_RTPRIV_IOCTL_ADAPTER_RT28XX_USB_ASICRADIO_OFF:
			ASIC_RADIO_OFF(pAd, SUSPEND_RADIO_OFF);
			break;

		case CMD_RTPRIV_IOCTL_ADAPTER_RT28XX_USB_ASICRADIO_ON:
			ASIC_RADIO_ON(pAd, RESUME_RADIO_ON);
			break;

#endif
//#endif /* CONFIG_PM */

		case CMD_RTPRIV_IOCTL_AP_BSSID_GET:
			if (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED)
				memcpy(pData, pAd->MlmeAux.Bssid, 6);
			else
				return NDIS_STATUS_FAILURE;
			break;
#endif /* CONFIG_STA_SUPPORT */

		case CMD_RTPRIV_IOCTL_SANITY_CHECK:
		/* sanity check before IOCTL */
			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE)))
			{
				if(pData == NULL ||	RT_isLegalCmdBeforeInfUp((char *) pData) == false)
				return NDIS_STATUS_FAILURE;
			}
			break;

		case CMD_RTPRIV_IOCTL_SIOCGIWFREQ:
		/* get channel number */
			*(ULONG *)pData = pAd->CommonCfg.Channel;
			break;



		case CMD_RTPRIV_IOCTL_BEACON_UPDATE:
		/* update all beacon contents */
			break;

		case CMD_RTPRIV_IOCTL_RXPATH_GET:
		/* get the number of rx path */
			*(ULONG *)pData = pAd->Antenna.field.RxPath;
			break;

		case CMD_RTPRIV_IOCTL_CHAN_LIST_NUM_GET:
			*(ULONG *)pData = pAd->ChannelListNum;
			break;

		case CMD_RTPRIV_IOCTL_CHAN_LIST_GET:
		{
			u32 i;
			u8 *pChannel = (u8 *)pData;

			for (i = 1; i <= pAd->ChannelListNum; i++)
			{
				*pChannel = pAd->ChannelList[i-1].Channel;
				pChannel ++;
			}
		}
			break;

		case CMD_RTPRIV_IOCTL_FREQ_LIST_GET:
		{
			u32 i;
			u32 *pFreq = (u32 *)pData;
			u32 m;

			for (i = 1; i <= pAd->ChannelListNum; i++)
			{
				m = 2412000;
				MAP_CHANNEL_ID_TO_KHZ(pAd->ChannelList[i-1].Channel, m);
				(*pFreq) = m;
				pFreq ++;
			}
		}
			break;

#ifdef EXT_BUILD_CHANNEL_LIST
       case CMD_RTPRIV_SET_PRECONFIG_VALUE:
       /* Set some preconfigured value before interface up*/
           pAd->CommonCfg.DfsType = MAX_RD_REGION;
           break;
#endif /* EXT_BUILD_CHANNEL_LIST */


#ifdef RTMP_USB_SUPPORT
		case CMD_RTPRIV_IOCTL_USB_CONFIG_INIT:
		{
			RT_CMD_USB_DEV_CONFIG *pConfig;
			u32 i;
			pConfig = (RT_CMD_USB_DEV_CONFIG *)pData;

			pAd->NumberOfPipes = pConfig->NumberOfPipes;
			pAd->BulkInMaxPacketSize = pConfig->BulkInMaxPacketSize;
			pAd->BulkOutMaxPacketSize = pConfig->BulkOutMaxPacketSize;

			for (i = 0; i < 6; i++)
				pAd->BulkOutEpAddr[i] = pConfig->BulkOutEpAddr[i];

			for (i = 0; i < 2; i++)
				pAd->BulkInEpAddr[i] = pConfig->BulkInEpAddr[i];
		}
			break;

		case CMD_RTPRIV_IOCTL_USB_SUSPEND:
			pAd->PM_FlgSuspend = 1;
			if (Data)
			{
				RTUSBCancelPendingBulkInIRP(pAd);
				RTUSBCancelPendingBulkOutIRP(pAd);
			}
			break;

		case CMD_RTPRIV_IOCTL_USB_RESUME:
			pAd->PM_FlgSuspend = 0;
			break;
#endif /* RTMP_USB_SUPPORT */


#ifdef RT_CFG80211_SUPPORT
		case CMD_RTPRIV_IOCTL_CFG80211_CFG_START:
			RT_CFG80211_REINIT(pAd);
			RT_CFG80211_CRDA_REG_RULE_APPLY(pAd);
			break;
#endif /* RT_CFG80211_SUPPORT */

		case CMD_RTPRIV_IOCTL_VIRTUAL_INF_UP:
		/* interface up */
		{
			RT_CMD_INF_UP_DOWN *pInfConf = (RT_CMD_INF_UP_DOWN *)pData;

			if (VIRTUAL_IF_NUM(pAd) == 0)
			{
				if (pInfConf->rt28xx_open(pAd->net_dev) != 0)
				{
					DBGPRINT(RT_DEBUG_TRACE, ("rt28xx_open return fail!\n"));
					return NDIS_STATUS_FAILURE;
				}
			}
			else
			{
			}
			VIRTUAL_IF_INC(pAd);
		}
			break;

		case CMD_RTPRIV_IOCTL_VIRTUAL_INF_DOWN:
		/* interface down */
		{
			RT_CMD_INF_UP_DOWN *pInfConf = (RT_CMD_INF_UP_DOWN *)pData;

			VIRTUAL_IF_DEC(pAd);
			if (VIRTUAL_IF_NUM(pAd) == 0)
				pInfConf->rt28xx_close(pAd->net_dev);
		}
			break;

		case CMD_RTPRIV_IOCTL_VIRTUAL_INF_GET:
		/* get virtual interface number */
			*(ULONG *)pData = VIRTUAL_IF_NUM(pAd);
			break;

		case CMD_RTPRIV_IOCTL_INF_TYPE_GET:
		/* get current interface type */
			*(ULONG *)pData = pAd->infType;
			break;

		case CMD_RTPRIV_IOCTL_INF_STATS_GET:
			/* get statistics */
			{
				RT_CMD_STATS *pStats = (RT_CMD_STATS *)pData;
				pStats->pStats = pAd->stats;
				if(pAd->OpMode == OPMODE_STA)
				{
					pStats->rx_packets = pAd->WlanCounters.ReceivedFragmentCount.QuadPart;
					pStats->tx_packets = pAd->WlanCounters.TransmittedFragmentCount.QuadPart;
					pStats->rx_bytes = pAd->RalinkCounters.ReceivedByteCount;
					pStats->tx_bytes = pAd->RalinkCounters.TransmittedByteCount;
					pStats->rx_errors = pAd->Counters8023.RxErrors;
					pStats->tx_errors = pAd->Counters8023.TxErrors;
					pStats->multicast = pAd->WlanCounters.MulticastReceivedFrameCount.QuadPart;   /* multicast packets received*/
					pStats->collisions = pAd->Counters8023.OneCollision + pAd->Counters8023.MoreCollisions;  /* Collision packets*/
					pStats->rx_over_errors = pAd->Counters8023.RxNoBuffer;                   /* receiver ring buff overflow*/
					pStats->rx_crc_errors = 0;/*pAd->WlanCounters.FCSErrorCount;      recved pkt with crc error*/
					pStats->rx_frame_errors = pAd->Counters8023.RcvAlignmentErrors;          /* recv'd frame alignment error*/
					pStats->rx_fifo_errors = pAd->Counters8023.RxNoBuffer;                   /* recv'r fifo overrun*/
				}
			}
			break;

		case CMD_RTPRIV_IOCTL_INF_IW_STATUS_GET:
		/* get wireless statistics */
		{
			u8 CurOpMode = OPMODE_AP;
			struct RT_CMD_IW_STATS *pStats = (struct RT_CMD_IW_STATS *)pData;

			pStats->qual = 0;
			pStats->level = 0;
			pStats->noise = 0;
			pStats->pStats = pAd->iw_stats;

#ifdef CONFIG_STA_SUPPORT
			if (pAd->OpMode == OPMODE_STA)
			{
				CurOpMode = OPMODE_STA;
			}
#endif /* CONFIG_STA_SUPPORT */

			/*check if the interface is down*/
			if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
				return NDIS_STATUS_FAILURE;


#ifdef CONFIG_STA_SUPPORT
			if (CurOpMode == OPMODE_STA)
				pStats->qual = ((pAd->Mlme.ChannelQuality * 12)/10 + 10);
#endif /* CONFIG_STA_SUPPORT */

			if (pStats->qual > 100)
				pStats->qual = 100;

#ifdef CONFIG_STA_SUPPORT
			if (CurOpMode == OPMODE_STA)
			{
				pStats->level =
					RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.AvgRssi0,
									pAd->StaCfg.RssiSample.AvgRssi1,
									pAd->StaCfg.RssiSample.AvgRssi2);
			}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
			pStats->noise = RTMPMaxRssi(pAd, pAd->StaCfg.RssiSample.AvgRssi0,
										pAd->StaCfg.RssiSample.AvgRssi1,
										pAd->StaCfg.RssiSample.AvgRssi2) -
										RTMPMinSnr(pAd, pAd->StaCfg.RssiSample.AvgSnr0,
										pAd->StaCfg.RssiSample.AvgSnr1);
#endif /* CONFIG_STA_SUPPORT */
		}
			break;

		case CMD_RTPRIV_IOCTL_INF_MAIN_CREATE:
			*(void **)pData = RtmpPhyNetDevMainCreate(pAd);
			break;

		case CMD_RTPRIV_IOCTL_INF_MAIN_CHECK:
			if (Data != INT_MAIN)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_INF_P2P_CHECK:
			if (Data != INT_P2P)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_MAC_ADDR_GET:

			RT28xx_EEPROM_READ16(pAd, 0x04, Addr01);
			RT28xx_EEPROM_READ16(pAd, 0x06, Addr23);
			RT28xx_EEPROM_READ16(pAd, 0x08, Addr45);

			PermanentAddress[0] = (u8)(Addr01 & 0xff);
			PermanentAddress[1] = (u8)(Addr01 >> 8);
			PermanentAddress[2] = (u8)(Addr23 & 0xff);
			PermanentAddress[3] = (u8)(Addr23 >> 8);
			PermanentAddress[4] = (u8)(Addr45 & 0xff);
			PermanentAddress[5] = (u8)(Addr45 >> 8);

			for(i=0; i<6; i++)
				*(u8 *)(pData+i) = PermanentAddress[i];
			break;

		case CMD_RTPRIV_IOCTL_SIOCGIWNAME:
			RtmpIoctl_rt_ioctl_giwname(pAd, pData, 0);
			break;

	}

#ifdef RT_CFG80211_SUPPORT
	if ((CMD_RTPRIV_IOCTL_80211_START <= cmd) &&
		(cmd <= CMD_RTPRIV_IOCTL_80211_END))
	{
		CFG80211DRV_IoctlHandle(pAd, wrq, cmd, subcmd, pData, Data);
	}
#endif /* RT_CFG80211_SUPPORT */

	if (cmd >= CMD_RTPRIV_IOCTL_80211_COM_LATEST_ONE)
		return NDIS_STATUS_FAILURE;

	return Status;
}

/*
    ==========================================================================
    Description:
        Issue a site survey command to driver
	Arguments:
	    pAdapter                    Pointer to our adapter
	    wrq                         Pointer to the ioctl argument

    Return Value:
        None

    Note:
        Usage:
               1.) iwpriv ra0 set site_survey
    ==========================================================================
*/
INT Set_SiteSurvey_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	NDIS_802_11_SSID Ssid;
	struct os_cookie *pObj;

	pObj = pAd->OS_Cookie;

	//check if the interface is down
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		if (MONITOR_ON(pAd))
    	{
        	DBGPRINT(RT_DEBUG_TRACE, ("!!! Driver is in Monitor Mode now !!!\n"));
        	return -EINVAL;
    	}
	}
#endif // CONFIG_STA_SUPPORT //

    memset(&Ssid, 0, sizeof(NDIS_802_11_SSID));


#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		Ssid.SsidLength = 0;
		if ((arg != NULL) &&
			(strlen(arg) <= MAX_LEN_OF_SSID))
		{
			memmove(Ssid.Ssid, arg, strlen(arg));
			Ssid.SsidLength = strlen(arg);
		}

		pAd->StaCfg.bSkipAutoScanConn = true;
		StaSiteSurvey(pAd, &Ssid, SCAN_ACTIVE);
	}
#endif // CONFIG_STA_SUPPORT //

	DBGPRINT(RT_DEBUG_TRACE, ("Set_SiteSurvey_Proc\n"));

    return true;
}

#ifdef MT76x0
INT set_temp_sensor_proc(
	IN struct rtmp_adapter	*pAd,
	IN char *		arg)
{
	if (simple_strtol(arg, 0, 10) == 0) {
		pAd->chipCap.bDoTemperatureSensor = false;
		pAd->chipCap.LastTemperatureforVCO = 0x7FFF;
		pAd->chipCap.LastTemperatureforCal = 0x7FFF;
		pAd->chipCap.NowTemperature = 0x7FFF;
	}
	else
		pAd->chipCap.bDoTemperatureSensor = true;

	DBGPRINT(RT_DEBUG_OFF, ("%s:: bDoTemperatureSensor = %d \n", __FUNCTION__, pAd->chipCap.bDoTemperatureSensor));

	return true;
}
#endif /* MT76x0 */

