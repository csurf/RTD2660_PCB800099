#include "Core\Header\Include.h"

#if(_TV_CHIP == _TV_JS_6B2)





 
//==========================================================================
//                            kx_CI2CWriteIfPllDM
//==========================================================================
void kx_CI2CWriteIfPllDM(BYTE ucTvType, const bit bTunerMute, const BYTE ucAFTMode)
{
    if (ucAFTMode);

    // Set TDA9886 B,C data
    switch(ucTvType)
    {
    default:                // Normal sound
    case _TV_PAL_BG    :    // 50Hz PAL B/G
    case _TV_PAL_I     :    // 50Hz PAL I
    case _TV_PAL_DK    :    // 50Hz PAL D/K
        pData[0] = _TDA9886_B_DATA_PAL;
        pData[1] = _TDA9886_C_DATA_PAL;
        break;

    case _TV_SECAM_L   :    // 50Hz SECAM L
        pData[0] = _TDA9886_B_DATA_SECAML;
        pData[1] = _TDA9886_C_DATA_SECAML;
        break;

    case _TV_SECAM_LL  :    // 50Hz SECAM L'
        pData[0] = _TDA9886_B_DATA_SECAMLL;
        pData[1] = _TDA9886_C_DATA_SECAMLL;
        break;

    } // end switch

    if (bTunerMute)
        CMuteOn();
    
    // Set TDA9886 E data
    switch(ucTvType)
    {
    case _TV_PAL_BG    :    // Sound 5.5M  PAL B/G
        pData[2] = _TDA9886_E_DATA_PAL_BG;
        break;

    default:                // Normal sound
    case _TV_PAL_I     :    // Sound 6.0M  PAL I
        pData[2] = _TDA9886_E_DATA_PAL_I;
        break;

    case _TV_PAL_DK    :    // Sound 6.5M  PAL D/K
        pData[2] = _TDA9886_E_DATA_PAL_DK;
        break;

    case _TV_SECAM_L   :    // Sound 6.5M  SECAM L
        pData[2] = _TDA9886_E_DATA_SECAML;
        break;

    case _TV_SECAM_LL  :    // Sound 6.5M  SECAM L'
        pData[2] = _TDA9886_E_DATA_SECAMLL;
        break;

    } // end switch
      
    CI2cWrite(_ADDR_IfPllDM,0x00,3, pData);  // Set TDA9886
}

//==========================================================================
//                         kx_CSetTuner
//==========================================================================
void kx_CSetTuner(BYTE ucTunerAddress, WORD Freq)
{	   
   pData[0] = 8;
   pData[1] = ucTunerAddress;
   ((WORD *)pData)[1] = Freq;

   pData[4] = _TUNER_CONTROL_BYTE1; 
  
   if(Freq < _VHF_LOW_BAND)  
      pData[5]=_TUNER_LOW_BAND;   // VHFLOW   
   else if(Freq < _VHF_HIGH_BAND)
      pData[5]=_TUNER_MID_BAND;   // VHFHIGH 
   else 
      pData[5]=_TUNER_HIGH_BAND;  //Band select//UHF 
   pData[6]=_TUNER_CONTROL_BYTE3;
   pData[7]=_TUNER_CONTROL_BYTE4;

   kx_CWriteTuner(pData);
}


//==========================================================================
//                         CTvAutoSearch
//==========================================================================
void CTvAutoSearch(void) small
{
    BYTE ucSound      = 0;
    BYTE ucPRG        = 1;
    BYTE ucmaxchannel = _GET_MAX_CHANNEL();
    WORD iFreq        = _MIN_FREQ;
       
    // Read color and sound type
    if (0 == ucmaxchannel)
    {
        ucSound  = 0;
        CLoadTVModeData(1);
    }
    else
    {
        ucSound  = 0;
        CLoadTVModeData(stTvInfo.CurChn);
    }

    // Reset search max tv channel number
    _SET_MAX_CHANNEL(1);
    stTvInfo.CurChn = 1;
	ucPrevChannel   = 1;
    // Save current channel
    CEepromSaveTvData();
//    CEepromSaveVedioColor();    

    kx_CI2CWriteIfPllDM(ucTVType, _TUNER_MUTE_ON, _NORMAL_MODE);
    CTimerDelayXms(8);
    CShowAutoSerachTotal(ucPRG - 1);   // Show search tv channel number
                    
#if(_AUTO_SERACH_METHOD == _AUTO_SEARCH_METHOD_1)
    CScalerSetBit(_VDISP_CTRL_28, ~_BIT5, 0x00); 
#endif

	CScalerPageSelect(_PAGE9);		
    CScalerSetByte(_P9_C_LOCK_CONF_B6, 0x6B);
    CTimerDelayXms(200);

    while(1)
    {
        iFreq = CLockChannel(iFreq+40, 1);

        if(iFreq < 2)
        {
            break;
        }
        
        _SET_CH_FREQ(iFreq);
        _SET_CH_SKIP(0);
        CSaveTVModeData(ucPRG);
        
        ucPRG++;
        if(ucPRG > _MAX_CHANNEL_COUNT)
        {
           ucPRG -= 1;
           break;
        }

        CShowAutoSerachTotal(ucPRG - 1);  // Show search tv channel number

    }  // End while

    {
    #if(_CHANGE_CHANNEL_MTD == _CHANGE_CHANNEL_M2)
        {
            BYTE ucChannelCount = ucPRG;
       
            for(; ucChannelCount < _MAX_CHANNEL_COUNT; ucChannelCount++)
            {
                _SET_CH_COLOR_TYPE(ucTVType);
                _SET_CH_FREQ((WORD)_MIN_FREQ);
                _SET_CH_SKIP(1);
                CSaveTVModeData(ucChannelCount);
            }
    
        }
    #endif
    
        // Save search channel number
        pData[0] = ucPRG;

        _SET_MAX_CHANNEL(pData[0]);
        CEepromSaveTvData();
    }

    CTimerDelayXms(10);
    CSetTVChannel(1);

    kx_CI2CWriteIfPllDM(ucTVType, _TUNER_MUTE_OFF, _NORMAL_MODE); // set tv system
    ucCurrState = _SEARCH_STATE;                  
}

//==========================================================================
//bSearchAction:   0 --> Search down
//                 1 --> Search up
//==========================================================================
void CManualSearch(const bit bSearchAction, const BYTE ucCurrentChannel) small
{
    BYTE ucSound           = 0;
    WORD iFreq             = 0;
    WORD ucSatrtSearchFreq = 0;  
   
    CMuteOn();
	CScalerPageSelect(_PAGE9);		
    CScalerSetByte(_P9_C_LOCK_CONF_B6, 0x6B);
    ucSatrtSearchFreq = _GET_CH_FREQ();

    kx_CI2CWriteIfPllDM(ucTVType, _TUNER_MUTE_ON, _NORMAL_MODE);
    CTimerDelayXms(100);

    if (ucSatrtSearchFreq > _MAX_FREQ)
        ucSatrtSearchFreq = _MAX_FREQ;
    else if(ucSatrtSearchFreq < _MIN_FREQ)
        ucSatrtSearchFreq = _MIN_FREQ;


    if (bSearchAction)
        iFreq = CLockChannel(ucSatrtSearchFreq + 40, 1);
    else
        iFreq = CLockChannel(ucSatrtSearchFreq - 40, 0);

    if (iFreq > 100)
    {
        _SET_CH_FREQ(iFreq);
        _SET_CH_SKIP(0);
        CSaveTVModeData(ucCurrentChannel);
    }
    CSetTVChannel(ucCurrentChannel);

    CShowAutoSearchSliderInOSD(_GET_CH_FREQ());                         

    CTimerDelayXms(50);
    if (kx_CVideoModeLocked())
        CScalerSetBit(_VDISP_CTRL_28, ~_BIT5, 0x00); 

//	CTimerReactiveTimerEvent(SEC(GET_OSDTIMEOUT()), COsdFxExitEvent);

    kx_CI2CWriteIfPllDM(ucTVType, _TUNER_MUTE_OFF, _NORMAL_MODE);
   // CModeResetTVMode();
    ucCurrState = _SEARCH_STATE;
}

//==========================================================================
//  bCLockMode --> 1: lock up         0 : lock down
//==========================================================================
WORD CLockChannel(WORD iStartFreq, const bit bCLockMode) small
{
    BYTE f            = 0;
    BYTE rData        = 0;
    BYTE LowLevel_CH  = 0;
    BYTE count        = 0;
    WORD freq         = 0;
    WORD iStartFreqBase = iStartFreq;
    BYTE ucPattCount  = 0;

    while(1)
    {
        kx_CSetTuner(_ADDR_TUNER, iStartFreq);
         
        if (bCLockMode) // Search up
        {
            if (iStartFreq > (iStartFreqBase + 300))
            {
                if(CKeyStopAutoSearch())
                   return 0;
            }
        }
        else
        {
            if (iStartFreq < (iStartFreqBase - 300))
            {
                if(CKeyStopAutoSearch())
                   return 0;
            }
        } 
        
        // Show search channel state 
        CShowAutoSearchSliderInOSD((iStartFreq));                         

        rData = kx_CReadIfPllDM();

        switch((rData >> 1) & 0x0f)
        {
        case 7:
            if (bCLockMode) // Search up
            {
                f = 1;
                iStartFreq++;
            }
            else            // Search down
            {
                count++;
                f |= 0x02;
            }
            break;

        case 8:
            if (bCLockMode) // Search up
            {
                count++;
                f |= 0x02;
            }
            else            // Search down
            {
                f = 1;
                iStartFreq--;
            }
            break; 

        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            f |= 0x0a;
            break; 
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            f |= 0x10;
            break; 
        case 0:
        case 15:
            f |= 0x04;
            freq = iStartFreq;
            break;
        }

		if (0x1b==f || 0x1a==f) 
		{
			LowLevel_CH++;
            if (0 == freq)
        		freq = iStartFreq;
		}
		else
            LowLevel_CH = 0;

        if((rData & 0xc0) != 0xc0)
        {
            f     = 0;
   	        count = 0;
      	  //  i    += 4;    
        }
        else if((f==0x1f || LowLevel_CH>4) && ((count > 3 && bCLockMode) || (count > 0 && !bCLockMode)) )
        {
            kx_CSetTuner(_ADDR_TUNER, freq);
			CTimerDelayXms(200);
            CVideoSoftReset();
            CTimerDelayXms(100);

            switch(CDetectTVSignalType())
            {
            case 0:  // MODE_NOSIGNAL
                LowLevel_CH = 0;  // eric add
                f = 0;
                count = 0;
                break;

            case 1:  // 
                return freq;
            }
            freq = 0;
        }

        if (bCLockMode) // Search up
        {
            iStartFreq++;
            if (iStartFreq > _MAX_FREQ)
                return 1;
        }
        else            // Search down
        {
            iStartFreq--;
            if (iStartFreq < _MIN_FREQ)
                return 1;
        }
    }
    return 1;  //Not Find Channel
}

//==========================================================================
//                        CSetTVChannel
//==========================================================================
void CSetTVChannel(const BYTE ucCurrentChannel)
{  
    CLoadTVModeData(ucCurrentChannel);    
        
    kx_CSetTuner(_ADDR_TUNER, _GET_CH_FREQ());
    kx_CI2CWriteIfPllDM(ucTVType, _TUNER_MUTE_ON, _NORMAL_MODE); // set tv system
    CTimerDelayXms(20);
    ucAudioState = 1;
#if (_TV_AFC)
    CurrAFCFreq = _GET_CH_FREQ();
    AFCState    = _TV_AFC_START;
#endif

    CSetTvColor(ucTVType);
    if (bChangeChannel == 1)
    {
	    CVideoSoftReset();
	    CTimerDelayXms(600);
    }
    ucTVSyncFailCount = 251;
}


#if (_TV_AFC)
//==========================================================================
//                        CTVAFC
//==========================================================================
void CTVAFC(void)
{
    BYTE rData = 0;

    if (_GET_INPUT_SOURCE() != _SOURCE_VIDEO_TV ||
        ucOsdState != _MI_MENU_NONE || bTVNoSignal == 1 || !GET_AFC_MODE())
       return;

    if (_TV_AFC_TIMEOUT == AFCState)
    {
        CLoadTVModeData(ucCurrentChannel);    
        kx_CSetTuner(_ADDR_TUNER, _GET_CH_FREQ());
        AFCState = _TV_AFC_STOP;
    }
    else if(_TV_AFC_STOP == AFCState)
    {
        return;
    }

    rData = kx_CReadIfPllDM();

    switch((rData >> 1) & 0x0f)
    {
    case 8:  // > +187.5KHz
    case 9:  //   +162.5KHz
    case 10: //   +137.5KHz
    case 11: //   +112.5KHz
    case 12: //   +87.5KHz
    case 13: //   +62.5KHz
        if (CurrAFCFreq)
            CurrAFCFreq--;
        kx_CSetTuner(_ADDR_TUNER, CurrAFCFreq);
        break;

    case 2:  //   -62.5KHz
    case 3:  //   -87.5KHz
    case 4:  //   -112.5KHz
    case 5:  //   -137.5KHz
    case 6:  //   -162.5KHz
    case 7:  // > -187.5KHz
        CurrAFCFreq++;
        kx_CSetTuner(_ADDR_TUNER, CurrAFCFreq);
        break;

    case 0:   // -12.5KHz
    case 1:   // -37.5KHz
    case 14:  // +37.5KHz
    case 15:  // +12.5KHz
        return;
    }

    AFCState++;
}
#endif //_TV_AFC

#endif // #if(_TV_CHIP == _TV_JS_6B2)

