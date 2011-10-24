/*
 * ScaleEnvPointsDlg.h
 * -------------------
 * Purpose: Header file for envelope scaling dialog.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 */

#pragma once

struct modplug::tracker::modenvelope_t;
//=======================================
class CScaleEnvPointsDlg : public CDialog
//=======================================
{
public:

    CScaleEnvPointsDlg(CWnd* pParent, modplug::tracker::modenvelope_t *pEnv, int nCenter) : CDialog(IDD_SCALE_ENV_POINTS, pParent)
    {
    	m_pEnv = pEnv;
    	m_nCenter = nCenter;
    }

private:
    modplug::tracker::modenvelope_t *m_pEnv; //To tell which envelope to process.
    static float m_fFactorX, m_fFactorY;
    int m_nCenter;

protected:
    virtual void OnOK();
    virtual BOOL OnInitDialog();
};
