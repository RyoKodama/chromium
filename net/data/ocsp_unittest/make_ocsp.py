#!/usr/bin/python
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This script is called without any arguments to re-generate all of the *.pem
files in the script's parent directory.

"""

from pyasn1.codec.der import decoder, encoder
from pyasn1_modules import rfc2560, rfc2459
from pyasn1.type import univ, useful
import hashlib, datetime

from OpenSSL import crypto

import base64

NEXT_SERIAL = 0

# 1/1/2017 00:00 GMT
CERT_DATE = datetime.datetime(2017, 1, 1, 0, 0)
# 1/1/2018 00:00 GMT
CERT_EXPIRE = CERT_DATE + datetime.timedelta(days=365)
# 2/1/2017 00:00 GMT
REVOKE_DATE = datetime.datetime(2017, 2, 1, 0, 0)
# 3/1/2017 00:00 GMT
THIS_DATE = datetime.datetime(2017, 3, 1, 0, 0)
# 3/2/2017 00:00 GMT
PRODUCED_DATE = datetime.datetime(2017, 3, 2, 0, 0)
# 6/1/2017 00:00 GMT
NEXT_DATE = datetime.datetime(2017, 6, 1, 0, 0)

sha1oid = univ.ObjectIdentifier('1.3.14.3.2.26')
sha1rsaoid = univ.ObjectIdentifier('1.2.840.113549.1.1.5')
sha256oid = univ.ObjectIdentifier('2.16.840.1.101.3.4.2.1')
sha256rsaoid = univ.ObjectIdentifier('1.2.840.113549.1.1.11')


def SigAlgOid(sig_alg):
  if sig_alg == 'sha1':
    return sha1rsaoid
  return sha256rsaoid


def CreateCert(name, signer=None, ocsp=False):
  global NEXT_SERIAL
  pkey = crypto.PKey()
  pkey.generate_key(crypto.TYPE_RSA, 1024)
  cert = crypto.X509()
  cert.set_version(2)
  cert.get_subject().CN = name
  cert.set_pubkey(pkey)
  cert.set_serial_number(NEXT_SERIAL)
  NEXT_SERIAL += 1
  cert.set_notBefore(CERT_DATE.strftime('%Y%m%d%H%M%SZ'))
  cert.set_notAfter(CERT_EXPIRE.strftime('%Y%m%d%H%M%SZ'))
  if ocsp:
    cert.add_extensions(
        [crypto.X509Extension('extendedKeyUsage', False, 'OCSPSigning')])
  if signer:
    cert.set_issuer(signer[1].get_subject())
    cert.sign(signer[2], 'sha1')
  else:
    cert.set_issuer(cert.get_subject())
    cert.sign(pkey, 'sha1')
  asn1cert = decoder.decode(
      crypto.dump_certificate(crypto.FILETYPE_ASN1, cert),
      asn1Spec=rfc2459.Certificate())[0]
  if not signer:
    signer = [asn1cert]
  return (asn1cert, cert, pkey, signer[0])


def CreateExtension():
  ext = rfc2459.Extension()
  ext.setComponentByName('extnID', univ.ObjectIdentifier('1.2.3.4'))
  ext.setComponentByName('extnValue', 'DEADBEEF')

  return ext


CA = CreateCert('Test CA', None)
CA_LINK = CreateCert('Test OCSP Signer', CA, True)
CA_BADLINK = CreateCert('Test False OCSP Signer', CA, False)
CERT = CreateCert('Test Cert', CA)
JUNK_CERT = CreateCert('Random Cert', None)
EXTENSION = CreateExtension()


def GetName(c):
  rid = rfc2560.ResponderID()
  subject = c[0].getComponentByName('tbsCertificate').getComponentByName(
      'subject')
  rn = rid.componentType.getTypeByPosition(0).clone()
  for i in range(len(subject)):
    rn.setComponentByPosition(i, subject.getComponentByPosition(i))
  rid.setComponentByName('byName', rn)
  return rid


def GetKeyHash(c):
  rid = rfc2560.ResponderID()
  spk = c[0].getComponentByName('tbsCertificate').getComponentByName(
      'subjectPublicKeyInfo').getComponentByName('subjectPublicKey')
  keyHash = hashlib.sha1(encoder.encode(spk)[4:]).digest()
  rid.setComponentByName('byKey', keyHash)
  return rid


def CreateSingleResponse(cert=CERT,
                         status=0,
                         next=None,
                         revoke_time=None,
                         reason=None,
                         extensions=[]):
  sr = rfc2560.SingleResponse()
  cid = sr.setComponentByName('certID').getComponentByName('certID')

  issuer_tbs = cert[3].getComponentByName('tbsCertificate')
  tbs = cert[0].getComponentByName('tbsCertificate')
  name_hash = hashlib.sha1(
      encoder.encode(issuer_tbs.getComponentByName('subject'))).digest()
  key_hash = hashlib.sha1(
      encoder.encode(
          issuer_tbs.getComponentByName('subjectPublicKeyInfo')
          .getComponentByName('subjectPublicKey'))[4:]).digest()
  sn = tbs.getComponentByName('serialNumber')

  ha = cid.setComponentByName('hashAlgorithm').getComponentByName(
      'hashAlgorithm')
  ha.setComponentByName('algorithm', sha1oid)
  cid.setComponentByName('issuerNameHash', name_hash)
  cid.setComponentByName('issuerKeyHash', key_hash)
  cid.setComponentByName('serialNumber', sn)

  cs = rfc2560.CertStatus()
  if status == 0:
    cs.setComponentByName('good')
  elif status == 1:
    ri = cs.componentType.getTypeByPosition(1).clone()
    if revoke_time == None:
      revoke_time = REVOKE_DATE
    ri.setComponentByName('revocationTime',
                          useful.GeneralizedTime(
                              revoke_time.strftime('%Y%m%d%H%M%SZ')))
    if reason:
      ri.setComponentByName('revocationReason', reason)
    cs.setComponentByName('revoked', ri)
  else:
    ui = cs.componentType.getTypeByPosition(2).clone()
    cs.setComponentByName('unknown', ui)

  sr.setComponentByName('certStatus', cs)

  sr.setComponentByName('thisUpdate',
                        useful.GeneralizedTime(
                            THIS_DATE.strftime('%Y%m%d%H%M%SZ')))
  if next:
    sr.setComponentByName('nextUpdate', next.strftime('%Y%m%d%H%M%SZ'))
  if extensions:
    elist = sr.setComponentByName('singleExtensions').getComponentByName(
        'singleExtensions')
    for i in range(len(extensions)):
      elist.setComponentByPosition(i, extensions[i])
  return sr


def Create(signer=None,
           response_status=0,
           response_type='1.3.6.1.5.5.7.48.1.1',
           signature=None,
           version=1,
           responder=None,
           responses=None,
           extensions=None,
           certs=None,
           sigAlg='sha1'):
  ocsp = rfc2560.OCSPResponse()
  ocsp.setComponentByName('responseStatus', response_status)

  if response_status != 0:
    return ocsp

  tbs = rfc2560.ResponseData()
  if version != 1:
    tbs.setComponentByName('version', version)

  if not signer:
    signer = CA
  if not responder:
    responder = GetName(signer)
  tbs.setComponentByName('responderID', responder)
  tbs.setComponentByName('producedAt',
                         useful.GeneralizedTime(
                             PRODUCED_DATE.strftime('%Y%m%d%H%M%SZ')))
  rlist = tbs.setComponentByName('responses').getComponentByName('responses')
  if responses == None:
    responses = [CreateSingleResponse(CERT, 0)]
  if responses:
    for i in range(len(responses)):
      rlist.setComponentByPosition(i, responses[i])

  if extensions:
    elist = tbs.setComponentByName('responseExtensions').getComponentByName(
        'responseExtensions')
    for i in range(len(extensions)):
      elist.setComponentByPosition(i, extensions[i])

  sa = rfc2459.AlgorithmIdentifier()
  sa.setComponentByName('algorithm', SigAlgOid(sigAlg))
  sa.setComponentByName('parameters', univ.Null())

  basic = rfc2560.BasicOCSPResponse()
  basic.setComponentByName('tbsResponseData', tbs)
  basic.setComponentByName('signatureAlgorithm', sa)
  if not signature:
    signature = crypto.sign(signer[2], encoder.encode(tbs), sigAlg)
  basic.setComponentByName('signature',
                           univ.BitString("'%s'H" % (signature.encode('hex'))))
  if certs:
    cs = basic.setComponentByName('certs').getComponentByName('certs')
    for i in range(len(certs)):
      cs.setComponentByPosition(i, certs[i][0])

  rbytes = ocsp.componentType.getTypeByPosition(1)
  rbytes.setComponentByName('responseType',
                            univ.ObjectIdentifier(response_type))
  rbytes.setComponentByName('response', encoder.encode(basic))

  ocsp.setComponentByName('responseBytes', rbytes)
  return ocsp


def Store(fname, description, ca, data):
  ca64 = crypto.dump_certificate(crypto.FILETYPE_PEM, ca[1]).replace(
      'CERTIFICATE', 'CA CERTIFICATE')
  c64 = crypto.dump_certificate(crypto.FILETYPE_PEM, CERT[1])
  d64 = base64.b64encode(encoder.encode(data))
  wd64 = '\n'.join(d64[pos:pos + 64] for pos in xrange(0, len(d64), 64))
  out = ('%s\n-----BEGIN OCSP RESPONSE-----\n%s\n'
         '-----END OCSP RESPONSE-----\n\n%s\n\n%s') % (description, wd64, ca64,
                                                       c64)
  open('%s.pem' % fname, 'w').write(out)


Store(
    'no_response',
    'No SingleResponses attached to the response',
    CA,
    Create(responses=[]))

Store(
    'malformed_request',
    'Has a status of MALFORMED_REQUEST',
    CA,
    Create(response_status=1))
Store(
    'bad_status',
    'Has an invalid status larger than the defined Status enumeration',
    CA,
    Create(response_status=17))
Store(
    'bad_ocsp_type',
    'Has an invalid OCSP OID',
    CA,
    Create(response_type='1.3.6.1.5.5.7.48.1.2'))
Store(
    'bad_signature',
    'Has an invalid signature',
    CA,
    Create(signature='\xde\xad\xbe\xef'))
Store('ocsp_sign_direct', 'Signed directly by the issuer', CA,
      Create(signer=CA, certs=[]))
Store('ocsp_sign_indirect', 'Signed indirectly through an intermediate', CA,
      Create(signer=CA_LINK, certs=[CA_LINK]))
Store('ocsp_sign_indirect_missing',
      'Signed indirectly through a missing intermediate', CA,
      Create(signer=CA_LINK, certs=[]))
Store('ocsp_sign_bad_indirect',
      'Signed through an intermediate without the correct key usage', CA,
      Create(signer=CA_BADLINK, certs=[CA_BADLINK]))
Store('ocsp_extra_certs', 'Includes extra certs', CA,
      Create(signer=CA, certs=[CA, CA_LINK]))
Store('has_version', 'Includes a default version V1', CA, Create(version=1))
Store(
    'responder_name',
    'Uses byName to identify the signer',
    CA,
    Create(responder=GetName(CA)))
Store(
    'responder_id',
    'Uses byKey to identify the signer',
    CA,
    Create(responder=GetKeyHash(CA)))
Store(
    'has_extension',
    'Includes an x509v3 extension',
    CA,
    Create(extensions=[EXTENSION]))

Store(
    'good_response',
    'Is a valid response for the cert',
    CA,
    Create(responses=[CreateSingleResponse(CERT, 0)]))
Store('good_response_sha256',
      'Is a valid response for the cert with a SHA256 signature', CA,
      Create(responses=[CreateSingleResponse(CERT, 0)], sigAlg='sha256'))
Store(
    'good_response_next_update',
    'Is a valid response for the cert until nextUpdate',
    CA,
    Create(responses=[CreateSingleResponse(CERT, 0, next=NEXT_DATE)]))
Store(
    'revoke_response',
    'Is a REVOKE response for the cert',
    CA,
    Create(responses=[CreateSingleResponse(CERT, 1)]))
Store(
    'revoke_response_reason',
    'Is a REVOKE response for the cert with a reason',
    CA,
    Create(responses=[
        CreateSingleResponse(CERT, 1, revoke_time=REVOKE_DATE, reason=1)
    ]))
Store(
    'unknown_response',
    'Is an UNKNOWN response for the cert',
    CA,
    Create(responses=[CreateSingleResponse(CERT, 2)]))
Store(
    'multiple_response',
    'Has multiple responses for the cert',
    CA,
    Create(responses=[
        CreateSingleResponse(CERT, 0),
        CreateSingleResponse(CERT, 2)
    ]))
Store(
    'other_response',
    'Is a response for a different cert',
    CA,
    Create(responses=[
        CreateSingleResponse(JUNK_CERT, 0),
        CreateSingleResponse(JUNK_CERT, 1)
    ]))
Store(
    'has_single_extension',
    'Has an extension in the SingleResponse',
    CA,
    Create(responses=[
        CreateSingleResponse(CERT, 0, extensions=[CreateExtension()])
    ]))

Store('missing_response', 'Missing a response for the cert', CA,
      Create(response_status=0, responses=[]))
