/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#include "ns3/log.h"
#include "ns3/simulator.h"

#include "ns3/mc-ue-pdcp.h"
#include "ns3/lte-pdcp-header.h"
#include "ns3/lte-pdcp-sap.h"
#include "ns3/lte-pdcp-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("McUePdcp");

class McUePdcpSpecificLteRlcSapUser : public LteRlcSapUser
{
public:
  McUePdcpSpecificLteRlcSapUser (McUePdcp* pdcp);

  // Interface provided to lower RLC entity (implemented from LteRlcSapUser)
  virtual void ReceivePdcpPdu (Ptr<Packet> p);

private:
  McUePdcpSpecificLteRlcSapUser ();
  McUePdcp* m_pdcp;
};

McUePdcpSpecificLteRlcSapUser::McUePdcpSpecificLteRlcSapUser (McUePdcp* pdcp)
  : m_pdcp (pdcp)
{
}

McUePdcpSpecificLteRlcSapUser::McUePdcpSpecificLteRlcSapUser ()
{
}

void
McUePdcpSpecificLteRlcSapUser::ReceivePdcpPdu (Ptr<Packet> p)
{
  m_pdcp->DoReceivePdu (p);
}

///////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED (McUePdcp);

McUePdcp::McUePdcp ()
  : m_pdcpSapUser (0),
    m_rlcSapProvider (0),
    m_rnti (0),
    m_lcid (0),
    m_txSequenceNumber (0),
    m_rxSequenceNumber (0)
{
  NS_LOG_FUNCTION (this);
  m_pdcpSapProvider = new LtePdcpSpecificLtePdcpSapProvider<McUePdcp> (this);
  m_rlcSapUser = new McUePdcpSpecificLteRlcSapUser (this);
}

McUePdcp::~McUePdcp ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
McUePdcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::McUePdcp")
    .SetParent<Object> ()
    .SetGroupName("Lte")
    .AddTraceSource ("TxPDU",
                     "PDU transmission notified to the RLC.",
                     MakeTraceSourceAccessor (&McUePdcp::m_txPdu),
                     "ns3::McUePdcp::PduTxTracedCallback")
    .AddTraceSource ("RxPDU",
                     "PDU received.",
                     MakeTraceSourceAccessor (&McUePdcp::m_rxPdu),
                     "ns3::McUePdcp::PduRxTracedCallback")
    ;
  return tid;
}

void
McUePdcp::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete (m_pdcpSapProvider);
  delete (m_rlcSapUser);
}


void
McUePdcp::SetRnti (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  m_rnti = rnti;
}

void
McUePdcp::SetMmWaveRnti (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << (uint32_t) rnti);
  m_mmWaveRnti = rnti;
}

void
McUePdcp::SetLcId (uint8_t lcId)
{
  NS_LOG_FUNCTION (this << (uint32_t) lcId);
  m_lcid = lcId;
}

void
McUePdcp::SetLtePdcpSapUser (LtePdcpSapUser * s)
{
  NS_LOG_FUNCTION (this << s);
  m_pdcpSapUser = s;
}

LtePdcpSapProvider*
McUePdcp::GetLtePdcpSapProvider ()
{
  NS_LOG_FUNCTION (this);
  return m_pdcpSapProvider;
}

void
McUePdcp::SetLteRlcSapProvider (LteRlcSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_rlcSapProvider = s;
}

void
McUePdcp::SetMmWaveRlcSapProvider (LteRlcSapProvider * s)
{
  NS_LOG_FUNCTION (this << s);
  m_mmWaveRlcSapProvider = s;
}

LteRlcSapUser*
McUePdcp::GetLteRlcSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_rlcSapUser;
}

McUePdcp::Status 
McUePdcp::GetStatus ()
{
  Status s;
  s.txSn = m_txSequenceNumber;
  s.rxSn = m_rxSequenceNumber;
  return s;
}

void 
McUePdcp::SetStatus (Status s)
{
  m_txSequenceNumber = s.txSn;
  m_rxSequenceNumber = s.rxSn;
}

////////////////////////////////////////

void
McUePdcp::DoTransmitPdcpSdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());

  LtePdcpHeader pdcpHeader;
  pdcpHeader.SetSequenceNumber (m_txSequenceNumber);

  m_txSequenceNumber++;
  if (m_txSequenceNumber > m_maxPdcpSn)
    {
      m_txSequenceNumber = 0;
    }

  pdcpHeader.SetDcBit (LtePdcpHeader::DATA_PDU);

  NS_LOG_LOGIC ("PDCP header: " << pdcpHeader);
  p->AddHeader (pdcpHeader);

  // Sender timestamp
  PdcpTag pdcpTag (Simulator::Now ());
  p->AddByteTag (pdcpTag);
  m_txPdu (m_rnti, m_lcid, p->GetSize ());

  LteRlcSapProvider::TransmitPdcpPduParameters params;
  params.rnti = m_rnti;
  params.lcid = m_lcid;
  params.pdcpPdu = p;

  if(m_mmWaveRlcSapProvider == 0 || m_txSequenceNumber % 2 == 0)
  {
    m_rlcSapProvider->TransmitPdcpPdu (params);
  }
  else
  {
    m_mmWaveRlcSapProvider->TransmitPdcpPdu (params);
  }
}

void
McUePdcp::DoReceivePdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << m_rnti << (uint32_t) m_lcid << p->GetSize ());

  // Receiver timestamp
  PdcpTag pdcpTag;
  Time delay;
  if (p->FindFirstMatchingByteTag (pdcpTag))
    {
      delay = Simulator::Now() - pdcpTag.GetSenderTimestamp ();
    }
  m_rxPdu(m_rnti, m_lcid, p->GetSize (), delay.GetNanoSeconds ());

  LtePdcpHeader pdcpHeader;
  p->RemoveHeader (pdcpHeader);
  NS_LOG_LOGIC ("PDCP header: " << pdcpHeader);

  m_rxSequenceNumber = pdcpHeader.GetSequenceNumber () + 1;
  if (m_rxSequenceNumber > m_maxPdcpSn)
    {
      m_rxSequenceNumber = 0;
    }

  LtePdcpSapUser::ReceivePdcpSduParameters params;
  params.pdcpSdu = p;
  params.rnti = m_rnti;
  params.lcid = m_lcid;
  m_pdcpSapUser->ReceivePdcpSdu (params);
}


} // namespace ns3
