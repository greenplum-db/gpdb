#!/bin/sh

#Check for needed tools
UTILS="sed tar awk cat mkdir tail mv more"
for util in ${UTILS}; do
    which ${util} > /dev/null 2>&1
    if [ $? != 0 ] ; then
cat <<EOF
********************************************************************************
Error: ${util} was not found in your path.
       ${util} is needed to run this installer.
       Please add ${util} to your path before running the installer again.
       Exiting installer.
********************************************************************************
EOF
       exit 1
    fi
done

#Verify that tar in path is GNU tar. If not, try using gtar.
#If gtar is not found, exit.
TAR=
tar --version > /dev/null 2>&1
if [ $? = 0 ] ; then
    TAR=tar
else
    which gtar > /dev/null 2>&1
    if [ $? = 0 ] ; then
        gtar --version > /dev/null 2>&1
        if [ $? = 0 ] ; then
            TAR=gtar
        fi
    fi
fi
if [ -z ${TAR} ] ; then
cat <<EOF
********************************************************************************
Error: GNU tar is needed to extract this installer.
       Please add it to your path before running the installer again.
       Exiting installer.
********************************************************************************
EOF
    exit 1
fi
platform="sles"
arch=x86_64
if [ -f /etc/SuSE-release ]; then
    if [ `uname -m` != "${arch}" ] ; then
        echo "Installer will only install on ${platform} ${arch}"
        exit 1
    fi
else
    echo "Installer will only install on ${platform} ${arch}"
    exit 1
fi
SKIP=`awk '/^__END_HEADER__/ {print NR + 1; exit 0; }' "$0"`

more << EOF

********************************************************************************
You must read and accept the Pivotal Database license agreement
before installing
********************************************************************************

       ***  IMPORTANT INFORMATION - PLEASE READ CAREFULLY  ***

PIVOTAL GREENPLUM DATABASE END USER LICENSE AGREEMENT

IMPORTANT - READ CAREFULLY: This Software contains computer programs and
other proprietary material and information, the use of which is subject to
and expressly conditioned upon acceptance of this End User License
Agreement ("EULA").

This EULA is a legally binding document between you (meaning the person or
the entity that obtained the Software under the terms and conditions of
this EULA, is agreeing to be bound by the terms and conditions of this
EULA, and is referred to below as "You" or "Customer") and Pivotal (meaning
(i) Pivotal Software, Inc., if Customer is located in the United States;
and (ii) the local Pivotal sales subsidiary, if Customer is located in a
country outside the United States in which Pivotal has a local sales
subsidiary; and (iii) GoPivotal International Limited, if Customer is
located in a country outside the United States in which Pivotal does not
have a local sales subsidiary (in each case, referred to herein as
"Pivotal"). Unless Customer has entered into a written and separately
signed agreement with Pivotal that is currently in effect with respect to
the license of the Software and provision of Support Services and
Subscription Services, this EULA governs Customer's use of the Software and
the provision of Support Services and Subscription Services. Capitalized
terms have the meaning stated in the EULA.

If Customer does not have a currently enforceable, written and separately
signed Software license agreement directly with Pivotal or the Distributor
from whom Customer obtained this Software, then by clicking on the "Agree"
or "Accept" or similar button in this EULA, or proceeding with the
installation, downloading, use or reproduction of this Software, or
authorizing any other person to do so, You are representing to Pivotal that
You are (i) authorized to bind the Customer; and (ii) agreeing on behalf of
the Customer that the terms of this EULA shall govern the relationship of
the parties with regard to the subject matter in this EULA, and waiving any
rights, to the maximum extent permitted by applicable law, to any claim
anywhere in the world concerning the enforceability or validity of this
EULA.

If Customer has a currently enforceable, written and separately signed
Software license agreement directly with Pivotal or the Distributor from
whom Customer obtained this Software, then by clicking on the "Agree" or
"Accept" or similar button at the end of this EULA, or proceeding with the
installation, downloading, use or reproduction of this Software, or
authorizing any other person to do so, You are representing that You are
(i) authorized to bind the Customer; and (ii) agreeing on behalf of the
Customer that the terms of such written, signed agreement shall replace and
supersede the terms of this EULA and shall govern the relationship of the
parties with regard to this Software, and are waiving any rights, to the
maximum extent permitted by applicable law, to any claim anywhere in the
world concerning the enforceability or validity of such written signed
agreement.

If You do not have authority to agree to the terms of this EULA on behalf
of the Customer, or do not accept the terms of this EULA on behalf of the
Customer, click on the "Cancel" or "Decline" or other similar button and/or
immediately cease any further attempt to install, download or use this
Software for any purpose, and remove any partial or full copies made from
this Software.

1.	 DEFINITIONS

1.1.	"Affiliate" means a legal entity that is controlled by, controls,
or is under common "control" of Pivotal or You. "Control" means more than
50% of the voting power or ownership interests.

1.2.	"Confidential Information" means the terms of this EULA, Software,
and all confidential and proprietary information of Pivotal or Customer,
including without limitation, all business plans, product plans, financial
information, software, designs, and technical, business and financial data
of any nature whatsoever, provided that such information is marked or
designated in writing as "confidential," "proprietary," or with any other
similar term or designation. Confidential Information does not include
information that: (i) was publicly known and made generally available in
the public domain prior to the time of disclosure by the disclosing party;
(ii) becomes publicly known and made generally available after disclosure
by the disclosing party to the receiving party through no action or
inaction of the receiving party; (iii) is already in the possession of the
receiving party at the time of disclosure by the disclosing party as shown
by the receiving party's files and records immediately prior to the time of
disclosure; (iv) is obtained by the receiving party from a third party
without a breach of such third party's obligations of confidentiality; (v)
is independently developed by the receiving party without use of or
reference to the disclosing party's Confidential Information, as shown by
documents and other competent evidence in the receiving party's possession
and prepared contemporaneously with such independent development; or (vi)
is required by law to be disclosed by the receiving party, provided that
the receiving party gives the disclosing party prompt written notice of
such requirement prior to such disclosure and assistance in obtaining an
order protecting the information from public disclosure.

1.3.	"Distributor" means a reseller, distributor, system integrator,
service provider, independent software vendor, value-added reseller, OEM,
or other partner that is authorized by Pivotal to license Software to end
users. The term shall also refer to any third party duly authorized by a
Distributor to license Software to end users.

1.4.	"Documentation" means, collectively, the operating instructions,
release notes, media, printed materials, user manuals and/or help files for
the Software in electronic or written form.

1.5.	"Evaluation Software" means Software made available by Pivotal
directly to You for a limited period of time at no charge to enable You to
evaluate such Software prior to making a final decision on licensing or
purchasing such from Pivotal.

1.6.	"License Key" or "Compliance Key" means a serial number that
enables You to activate and use the Software, as applicable.

1.7.	"Open Source Software" or "OSS" means software components that are
licensed under a license approved by the Open Source Initiative or similar
open source or freeware license and which are included in, embedded,
utilized by, provided or distributed with the Software.

1.8.	"Pivotal Product Guide" means the notice by which Pivotal informs
Customer of product-specific usage rights and restrictions. The Pivotal
Product Guide may be delivered in writing attached to the applicable
Distributor quote, or otherwise in writing and/or a posting on the
applicable Pivotal website, currently located at
http://www.gopivotal.com/product-guide. The terms of the Pivotal Product
Guide in effect as of the date of the quote shall be deemed incorporated
into and made a part of this EULA. Each Pivotal Product Guide is dated and
is archived when it is superseded by a newer version. Pivotal shall not
change any Pivotal Product Guide retroactively with regard to any products
listed on an applicable quote issued prior to the date of the applicable
Pivotal Product Guide. At Customer’s request, Pivotal shall without undue
delay provide Customer with a copy of the applicable Pivotal Product Guide.

1.9.	"Software" means the Pivotal computer programs (listed on Pivotal’s
commercial price list) for which you obtain a license under an order or
quote (which specifies a perpetual, Subscription Services time-bound or
license Term), together with any Software Release that is provided to You
pursuant to a Support Services and/or Subscription Services contract and
that is not subject to a separate license agreement, and associated
Documentation.

1.10.	"Software Release" means any subsequent version of the Software
provided by Pivotal after initial delivery of the Software, but does not
include new Software products or services (as determined by Pivotal).

1.11.	"Subscription Services" means, during the Term set forth in the
applicable quote or order for such Software: (a) access to the Software
subject to the licensing terms and restrictions in the Pivotal Product
Guide; and (b) Support Services, which include any new software product
introduced and additional Software Releases, including major upgrades set
forth in this EULA or additional Software Releases for the products being
purchased on a when and if available basis during the Term. For the absence
of doubt, the unspecified software product rights during the Term are
solely for the Software set forth in this EULA.

1.12.	"Support Services" means the services available from Pivotal or
its designee which provides Software Releases and support services for
Software as set forth at http://www.gopivotal.com/support, as such schedule
may be updated by Pivotal from time to time.

1.13.	"Term" shall mean the period of time during which You are licensed
to use the Software (and/or the Subscription Services), as set forth in the
quote, or order, and the Software will be available for Your use and/or
access only for the duration of such Term.

2.	  EVALUATION SOFTWARE

2.1    This EULA shall also apply to Software (including any copies made by
or on behalf of Customer) and Documentation licensed to You for a limited
duration for the specific purpose of evaluation prior to making a final
decision on procurement ("Evaluation Software"). You can only use
Evaluation Software in a non-commercial, non-production environment and
only for a sixty (60) day period beginning on the day the Evaluation
Software is made available to You, unless otherwise agreed to in writing by
Pivotal or as set forth in the Pivotal Product Guide ("Evaluation Term").
The Evaluation Software, installation site and other transaction-specific
conditions shall be as mutually agreed in writing between Pivotal and
Customer.

2.2     Notwithstanding any deviating terms in this EULA, all licenses for
Evaluation Software expire at the end of the Evaluation Term. The right to
evaluate the Evaluation Software expires at the end of the Evaluation Term
or upon return of the Evaluation Software to Pivotal, whichever is earlier.

2.3    Without prejudice to any other limitations on Pivotal’s liability
set forth in this EULA (which shall also apply to Evaluation Software),
Evaluation Software is provided "AS IS" and any warranty or damage claims
against Pivotal in connection with Evaluation Software are hereby excluded,
except in the event of fraud or wilful misconduct of Pivotal.

2.4    Unless otherwise specifically agreed in writing by Pivotal, Pivotal
does not provide maintenance or support for any Evaluation Software.
CUSTOMER RECOGNIZES THAT EVALUATION SOFTWARE MAY HAVE DEFECTS OR
DEFICIENCIES WHICH CANNOT OR MAY NOT BE CORRECTED BY PIVOTAL. Pivotal shall
have no liability to Customer for any claim, suit, action or proceeding
("Claim(s)") brought by or against Customer alleging that any or all of the
Evaluation Software or its operation or use infringes any patent,
copyright, trade secret or other intellectual property or proprietary
right. In event of such a Claim, Pivotal retains the right to terminate
this EULA and take possession of the Evaluation Software. THIS SECTION
STATES PIVOTAL’S ENTIRE LIABILITY WITH RESPECT TO ALLEGED INFRINGEMENT OF
INTELLECTUAL PROPERTY OR PROPRIETARY RIGHTS BY ANY OR ALL OF THE EVALUATION
SOFTWARE OR ITS OPERATION OR USE.

3.	  GRANT AND USE RIGHTS FOR SOFTWARE

3.1   License Grant.  The Software is licensed, not sold. Pivotal grants
You a non-exclusive, non-transferable license, without rights to
sublicense, to use the Software in the country where You are invoiced in
accordance with the Documentation and the Pivotal license model set forth
in the applicable Pivotal Product Guide, for which You have paid the
applicable license fees. Software must be installed on equipment located in
the country where You are invoiced. You may allow third party consultants
or contractors to access and use the Software on Your behalf solely for
Your internal business operations, provided that they are bound by an
agreement with You protecting Pivotal’s intellectual property with terms no
less stringent than this EULA and You ensure that such third party use of
the Software complies with the terms of this EULA. You may make one backup,
unmodified copy of the Software solely for archival purposes. If You
upgrade or exchange the Software from a previous validly licensed version,
You must cease use of the prior version of that Software. You agree to
provide written certification of destruction of the previous version of the
Software upon Pivotal’s request.

3.2   Open Source Software.  Notwithstanding anything herein to the
contrary, Open Source Software is licensed to You under such OSS’s own
applicable license terms, which can be found in the
open_source_licenses.txt file included in the Software, or as applicable,
the corresponding source files for the Software available at
http://www.gopivotal.com/open-source. Customer is responsible for complying
with any third party terms and conditions applicable to such Open Source
Software. These OSS license terms may contain additional rights benefiting
You. The OSS license terms shall take precedence over this EULA to the
extent that this EULA imposes greater restrictions on You than the
applicable OSS license terms, solely with respect to such OSS.

3.3  Licensing Models.  Software is licensed for use only in accordance
with the commercial terms and restrictions of the Software’s relevant
licensing model, which are stated in the Pivotal Product Guide found
http://www.gopivotal.com/product-guide and/or attached to the quote from
Pivotal or Distributor.

3.4    Restrictions.  Without Pivotal’s prior written consent, Customer
must not, and must not allow any third party, to: (i) use Software in an
application services provider, service bureau, or similar capacity for
third parties; (ii) disclose to any third party the results of any
benchmarking testing or comparative or competitive analyses of Software
done by or on behalf of Customer except as otherwise permitted herein;
(iii) make available Software in any form to anyone other than Customer’s
employees or contractors reasonably acceptable to Pivotal and which require
access to use Software on behalf of Customer in a matter permitted by this
EULA; (iv) transfer or sublicense Software or Documentation to an Affiliate
or any third party (notwithstanding the foregoing restriction, You may use
the Software to deliver hosted services to your Affiliates as defined
herein); (v) use Software in conflict with the terms and restrictions of
the Software’s licensing model and other requirements specified in the
Pivotal Product Guide and/or Pivotal quote; (vi) except to the extent
permitted by applicable mandatory law, modify, translate, enhance, or
create derivative works from the Software, or reverse assemble or
disassemble, reverse engineer, decompile, or otherwise attempt to derive
source code from the Software; (vii) remove any copyright or other
proprietary notices on or in any copies of Software; or (viii) violate or
circumvent any technological restrictions within the Software or specified
in this EULA, such as via software or services.

3.5    Decompilation. Notwithstanding the foregoing, decompiling the
Software is permitted to the extent the laws of the country in which You
are using the Software give You the express right to do so to obtain
information necessary to render the Software interoperable with other
software; provided that You must first request such information from
Pivotal (at legal@gopivotal.com), provide all reasonably requested
information to allow Pivotal to assess Your claim, and Pivotal may, in its
discretion, either provide such interoperability information to You, impose
reasonable conditions, including a reasonable fee, on such use of the
Software, or offer to provide alternatives to ensure that Pivotal’s
proprietary rights in the Software are protected and to reduce any adverse
impact on Pivotal’s proprietary rights.

3.6    Benchmarking.  You may use the Software to conduct internal
performance testing and benchmarking studies. You may only publish or
otherwise distribute the results of such studies to third parties as
follows:  only if You provide a copy of Your study to
benchmark@gopivotal.com for approval prior to such publication and 
distribution.

3.7   Customer Responsibilities.  You are responsible for separately
obtaining any software, hardware or other technology required to operate
the Software and complying with any corresponding terms and conditions. 
You are solely responsible for all obligations to comply with laws
applicable to Your use of the Software including without limitation any
processing of personal data.

3.8   Data Collection and Usage.  You agree that Pivotal may collect, use,
store and transmit technical and related information about You, your use of
the Software including but not limited to server internet protocol address,
hardware identification, operating system, application software, peripheral
hardware, and Software usage statistics, to facilitate the provisioning of
updates, support, invoicing, online services to You.  You are responsible
for obtaining any consents required in order to enable Pivotal to exercise
the rights set forth in this Section 3.8, in each case in compliance with
applicable law.

3.9   Audit Rights. During the Term and for two (2) years after termination
or expiration of the EULA or Support Services and/or Subscription Services
for the Software, You agree to maintain accurate records as to your
installation and use of the Software sufficient to provide evidence of
compliance with the terms of this EULA. Pivotal, or an independent third
party designated by Pivotal, may audit, upon written notice to You, your
books, records, and computing devices to determine your compliance with
this EULA and your payment of the applicable license and Support Services
and/or Subscription Services fees, if any, for the Software. Pivotal may
conduct no more than one (1) audit in any twelve (12) month period. In the
event that any such audit reveals an underpayment by You of more than five
percent (5%) of the license amounts due to Pivotal in the period being
audited, or that You have breached any term of the EULA, then, in addition
to paying to Pivotal any underpayments for Software licenses and Support
Services and/or Subscription Services fees and any other remedies Pivotal
may have, You will promptly pay to Pivotal the audit costs incurred by
Pivotal.  Customer grants Pivotal the right to use license management
technology included in its Software in furtherance of the audit rights set
forth herein.

3.10  Reserved Rights. Pivotal retains all right, title, and interest in
and to the Software, and all related intellectual property rights. Pivotal
retains all rights not expressly granted to You in this EULA.

4.     TITLE, DELIVERY AND ACCEPTANCE. Title and risk of loss for physical
media containing Software shall transfer to Customer upon Pivotal’s
delivery to a carrier at Pivotal’s designated point of shipment
("Delivery"). Unless otherwise agreed, a common carrier shall be specified
by Pivotal. Software may be provided by (i) Delivery of physical media; or
(ii) electronic download (when so offered by Pivotal).  All Software will
be deemed to be delivered and accepted, meaning that Software operates in
substantial conformity to the Documentation upon (i) Delivery of the
physical media; or (ii) transmission of a notice of availability for
download (accompanied by the license key when required by Pivotal or its
Distributor). Notwithstanding such acceptance, Customer retains all rights
and remedies set forth in Section 9.1 of this EULA.

5.    SUPPORT SERVICES AND SUBSCRIPTION SERVICES. In the event you have
purchased Pivotal Support or Subscription Services you will be entitled to
any updates, upgrades or extensions or enhancements to the Software. These
Support or Subscription Services are subject to Pivotal’s then-current
terms and conditions for such Support or Subscription Services as further
described at http://www.gopivotal.com/support.  Subscription Services
includes Support Services and enables You to obtain unspecificied upgrades
and major releases of the Software product purchased under such
Subscription Service during the Subscription Term.

6.     SOFTWARE RELEASES. Customer shall use and deploy Software Releases
strictly in accordance with terms of the original license for the Software.

7.    TERMINATION.  Pivotal may terminate this EULA in its entirety
effective immediately upon written notice to You if: (a) You breach any
provision in Section 3.4 and do not cure the breach within ten (10) days
after receiving written notice thereof from Pivotal; (b) You fail to pay
any portion of the fees under an applicable order within ten (10) days
after receiving written notice from Pivotal that payment is past due; (c)
You suffer an insolvency or analogous event; (d) You breach any other
provision of this EULA and do not cure the breach within thirty (30) days
after receiving written notice thereof from Pivotal; or (e) You commit a
material breach of this EULA that is not capable of being cured.  In the
event of expiration or any termination of this EULA, You must remove and
destroy all copies of the Software, including all backup copies, from the
server, virtual machine, and all computers and terminals on which the
Software is installed or used and certify destruction of applicable
Software (including copies). Any obligations to pay fees incurred prior to
termination and Sections 1, 2.3, 3.4, 3.8, 3.9, 4, 7, 9, 10, 11, 12 and 13
of this EULA shall survive expiration or termination of this EULA for any
reason.

8.    IP INDEMNITY. Pivotal shall (i) defend Customer against any third
party claim that Software or Support Services infringes a patent or
copyright existing in the country in which Pivotal is located, the United
States of America or the European Union; and (ii) pay the resulting costs
and damages finally awarded against Customer by a court of competent
jurisdiction or the amounts stated in a written settlement negotiated by
Pivotal. The foregoing obligations are subject to the following: Customer
(a) notifies Pivotal promptly in writing of such claim; (b) grants Pivotal
sole control over the defense and settlement thereof; (c) reasonably
cooperates in response to an Pivotal request for assistance; and (d) is not
in material breach of this EULA. Should any such Software or Support
Service become, or in Pivotal’s opinion be likely to become, the subject of
such a claim, Pivotal may, at its option and expense, (1) procure for
Customer the right to make continued use thereof; (2) replace or modify
such so that it becomes non-infringing; (3) request return of the Software
and, upon receipt thereof; refund the price paid by Customer, less
straight-line depreciation based on a three (3) year useful life for
Software; or (4) discontinue the Support Service and refund the portion of
any pre-paid Support Service fee that corresponds to the period of Support
Service discontinuation. Pivotal shall have no liability to the extent that
the alleged infringement arises out of or relates to: (A) the use or
combination of Software or Support Service with third party products or
services; (B) use for a purpose or in a manner for which the Software or
Support Service was not designed; (C) any modification made by any person
other than Pivotal or its authorized representatives; (D) any modifications
to Software or Support Service made by Pivotal pursuant to Customer’s
specific instructions; (E) any technology owned or licensed by Customer
from third parties; or (F) use of any older version of the Software when
use of a newer Software Release made available to Customer would have
avoided the infringement. THIS SECTION STATES CUSTOMER’S SOLE AND EXCLUSIVE
REMEDY AND PIVOTAL’S ENTIRE LIABILITY FOR THIRD PARTY INFRINGEMENT CLAIMS.

9.    LIMITED WARRANTY AND LIMITATION OF LIABILITY. 

9.1	   Software
Warranty, Duration and Remedy.  Pivotal warrants to Customer that the
Software will, for a period of ninety (90) days following Delivery or
notice of availability for electronic download ("Warranty Period"),
substantially conform to the applicable Documentation, provided that the
Software: (i) has been properly installed and used at all times in
accordance with the applicable Documentation; and (ii) has not been
modified or added to by persons other than Pivotal or its authorized
representative. Pivotal will, at its own expense and as its sole obligation
and Customer’s exclusive remedy for any breach of this warranty, either
replace that Software or correct any reproducible error in that Software
reported to Pivotal by Customer in writing during the Warranty Period. If
Pivotal determines that it is unable to correct the error or replace the
Software, Pivotal will refund to Customer the amount paid by Customer for
that Software, in which case the license for that Software will terminate.

9.2  WARRANTY EXCLUSIONS. EXCEPT AS SET FORTH IN SECTION 9.1, PIVOTAL AND
ITS LICENSORS PROVIDE THE SOFTWARE WITHOUT ANY WARRANTIES OF ANY KIND,
EXPRESS, IMPLIED, STATUTORY, OR IN ANY OTHER PROVISION OF THIS EULA OR
COMMUNICATION WITH YOU, AND PIVOTAL AND ITS LICENSORS SPECIFICALLY DISCLAIM
ANY IMPLIED WARRANTIES OR CONDITIONS OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE, NON-INFRINGEMENT, TITLE, AND ANY WARRANTIES ARISING
FROM COURSE OF DEALING OR COURSE OF PERFORMANCE REGARDING OR RELATING TO
THE SOFTWARE, THE DOCUMENTATION, OR ANY MATERIALS FURNISHED OR PROVIDED TO
YOU UNDER THIS EULA. PIVOTAL AND ITS LICENSORS DO NOT WARRANT THAT THE
SOFTWARE WILL OPERATE UNINTERRUPTED OR THAT IT WILL BE FREE FROM DEFECTS OR
THAT THE SOFTWARE WILL MEET (OR IS DESIGNED TO MEET) YOUR BUSINESS
REQUIREMENTS.

9.3  LIMITATION OF LIABILITY. A.	Limitation on Direct Damages.  EXCEPT
WITH RESPECT TO CLAIMS ARISING UNDER SECTION 8 ABOVE, PIVOTAL’S TOTAL
LIABILITY AND CUSTOMER’S SOLE AND EXCLUSIVE REMEDY FOR ANY CLAIM OF ANY
TYPE WHATSOEVER, ARISING OUT OF SOFTWARE OR SERVICE PROVIDED HEREUNDER,
SHALL BE LIMITED TO PROVEN DIRECT DAMAGES CAUSED BY PIVOTAL’S SOLE
NEGLIGENCE IN AN AMOUNT NOT TO EXCEED (i) US$1,000,000.00 FOR DAMAGE TO
REAL OR TANGIBLE PERSONAL PROPERTY; AND (ii) THE PRICE PAID BY CUSTOMER TO
PIVOTAL FOR THE SPECIFIC SERVICE (CALCULATED ON AN ANNUAL BASIS, WHEN
APPLICABLE) OR SOFTWARE FROM WHICH SUCH CLAIM ARISES, FOR DAMAGE OF ANY
TYPE NOT IDENTIFIED IN (i) ABOVE OR OTHERWISE EXCLUDED HEREUNDER. B.	No
Indirect Damages.  EXCEPT WITH RESPECT TO CLAIMS REGARDING VIOLATION OF
PIVOTAL’S INTELLECTUAL PROPERTY RIGHTS OR CLAIMS ARISING UNDER SECTION 8
ABOVE, NEITHER CUSTOMER NOR PIVOTAL SHALL HAVE LIABILITY TO THE OTHER FOR
ANY SPECIAL, CONSEQUENTIAL, EXEMPLARY, INCIDENTAL, OR INDIRECT DAMAGES
(INCLUDING, BUT NOT LIMITED TO, LOSS OF PROFITS, REVENUES, DATA AND/OR
USE), EVEN IF ADVISED OF THE POSSIBILITY THEREOF. C.	Special Exclusion. 
IN JURISDICTIONS THAT DO NOT ALLOW LIMITATION OR EXCLUSION OF CONSEQUENTIAL
OR INCIDENTAL DAMAGES, ALL OR A PORTION OF SECTION 9.3A AND/OR 9.3 B ABOVE
MAY NOT APPLY.

9.4   Limitation Period. All claims must be made within (i) the time period
specified by applicable law; or (ii) eighteen (18) months after the cause
of action accrues if no such period is specified at law.

9.5   Regular Back-ups. As part of its obligation to mitigate damages,
Customer shall take reasonable data backup measures. In particular,
Customer shall backup data before Pivotal performs any remedial works,
upgrades, uploads or installs any new Software Release or otherwise works
on Customer’s production systems. To the extent Pivotal’s liability for
loss of data is not anyway excluded under this EULA, Pivotal shall in case
of data losses only be liable for the typical effort to recover the data
which would have accrued if Customer had appropriately backed up its data.

10.   CONFIDENTIALITY. Each party shall (i) use Confidential Information of
the other party only for the purposes of exercising rights or performing
obligations in connection with this EULA; and (ii) use at least reasonable
care to protect from disclosure to any third parties any Confidential
Information disclosed by the other party for a period commencing upon the
date of disclosure until three (3) years thereafter, except with respect to
Customer data stored within the Software to which Pivotal may have access
in connection with the provision of Support or Subscription Services, which
shall remain Confidential Information until or unless one of the exceptions
stated in the above definition of Confidential Information applies.
Notwithstanding the foregoing, either party may disclose Confidential
Information (a) to independent contractors performing services on its
behalf and Affiliates for the purpose of fulfilling its obligations or
exercising its rights hereunder as long as such Affiliates and independent
contractors performing services on its behalf comply with the foregoing;
and (b) if required by law provided the receiving party has given the
disclosing party prompt notice. Pivotal will not be responsible for
unauthorized disclosure of Customer data stored within the Software arising
from a data security breach.

11.    SOFTWARE-SPECIFIC TERMS AND CONDITIONS. In addition to the above
sections, the Software is subject to the specific license use rights and
terms and conditions located at http://www.gopivotal.com/product-guide. In
the event of any conflict between the Software-specific terms and
conditions set forth in the Pivotal Product Guide and those set forth in
this EULA, the Software-specific terms and conditions set forth in the
Pivotal Product Guide shall control.

12.    GENERAL

12.1   Construction. As used in this EULA: (a) the terms "include" and
"including" are meant to be inclusive and shall be deemed to mean "include
without limitation" or "including without limitation," (b) the word "or" is
disjunctive, but not necessarily exclusive, (c) words used herein in the
singular, where the context so permits, shall be deemed to include the
plural and vice versa, (d) references to "dollars" or "$" shall be to
United States dollars unless otherwise specified herein, (e) unless
otherwise specified, all references to days, months or years shall be
deemed to be preceded by the word "calendar." The headings of this EULA are
intended solely for convenience of reference and shall be given no effect
in the interpretation or construction of this EULA.

12.2  Governing Law. This EULA is governed by: (i) the laws of California
when Pivotal means Pivotal Software, Inc.; (ii) the laws of the country in
which the applicable Pivotal subsidiary is registered to do business when
Pivotal means the local Pivotal subsidiary; and (iii) the laws of Ireland
when Pivotal means GoPivotal International Limited; provided that in each
case, the foregoing shall exclude any conflict of law rules, and the U.N.
Convention on Contracts for the International Sale of Goods shall not
apply.

12.3   Notices. Any notice, consent or other communication to be given
under this EULA by any party shall be in writing and shall be either (a)
personally delivered, (b) mailed by registered or certified mail, postage
prepaid with return receipt requested, (c) delivered by prepaid overnight
express delivery service or same-day local courier service, or (d) via
e-mail transmission, with receipt confirmed or a confirming copy sent via
mail. Notices delivered personally, by overnight express delivery service,
by local courier service, facsimile transmission or email shall be deemed
given as of actual receipt. Mailed notices shall be deemed given seven (7)
Business Days after mailing.

12.4   Successors and Assigns. This EULA may not be assigned without the
express written consent of the other party, not to be unreasonably
withheld, conditioned or delayed, except that Pivotal may assign or
transfer this EULA, in whole or in part, without consent of Customer to any
successors-in-interest to all or substantially all of the business or
assets of Pivotal whether by merger, reorganization, asset sale or
otherwise, or to any Affiliates of Pivotal. Any purported transfer or
assignment in violation of this section is void. Subject to the foregoing
restrictions, the terms and conditions of this EULA shall inure to the
benefit of and be binding upon the respective permitted successors and
assigns of the parties.

12.5   Severability. If any provision of this EULA becomes or is declared
by a court of competent jurisdiction to be illegal, unenforceable, or void,
portions of such provision, or such provision in its entirety, to the
extent necessary, shall be severed from this EULA, and such court will
replace such illegal, void or unenforceable provision of this EULA with a
valid and enforceable provision that will achieve, to the extent possible,
the same economic, business and other purposes of the illegal, void or
unenforceable provision. The balance of this EULA shall be enforceable in
accordance with its terms.

12.6   Waiver. Failure to enforce a provision of this EULA will not
constitute a waiver.

12.7  Independent Contractor. The parties are independent contractors.
Nothing in this EULA shall be construed to create a joint venture,
partnership, or an agency relationship between the parties themselves or
between the parties and any third person. Except as expressly provided
herein, no party has the authority, without the other party’s prior written
approval, to bind or commit any other party in any way.

12.8   No Third-party Beneficiaries. This EULA is not intended to confer
upon any person other than the parties hereto any rights or remedies
hereunder.

12.9   Force Majeure. In the event that either party is prevented from
performing or is unable to perform any of its obligations under this EULA
due to any Act of God, fire, casualty, flood, earthquake, war, strike,
lockout, epidemic, destruction of production facilities, riot,
insurrection, material unavailability, unavailability or interruption of
telecommunications equipment or networks, or any other cause beyond the
reasonable control of the party invoking this section, and if such party
shall have used reasonable efforts to mitigate its effects, such party
shall give prompt written notice to the other party, its performance shall
be excused, and the time for the performance shall be extended for the
period of delay or inability to perform due to such occurrences.

12.10   Compliance with Laws; Export Control; Government Regulations. Each
party shall comply with all laws applicable to the actions contemplated by
this EULA. You acknowledge that the Software is of United States origin, is
provided subject to the U.S. Export Administration Regulations, may be
subject to the export control laws of the applicable territory, and that
diversion contrary to applicable export control laws is prohibited. You
represent that (1) you are not, and are not acting on behalf of, (a) any
person who is a citizen, national, or resident of, or who is controlled by
the government of any country to which the United States has prohibited
export transactions; or (b) any person or entity listed on the U.S.
Treasury Department list of Specially Designated Nationals and Blocked
Persons, or the U.S. Commerce Department Denied Persons List or Entity
List; and (2) you will not permit the Software to be used for, any purposes
prohibited by law, including, any prohibited development, design,
manufacture or production of missiles or nuclear, chemical or biological
weapons. If the Software and related documentation is licensed to the
United States government or any agency thereof, then the Software and
documentation will be deemed to be "commercial computer software" and
"commercial computer software documentation," respectively, pursuant to
DFARS Section 227.7202 and FAR Section 12.212, as applicable. Any use,
reproduction, release, performance, display or disclosure of the Software
and any related documentation by the U.S. Government will be governed
solely by this EULA and is prohibited except to the extent expressly
permitted by this EULA.

12.11   Order of Precedence. In the event of conflict or inconsistency
among the Pivotal Product Guide, this EULA and a purchase order, the
following order of precedence shall apply: (a) the Pivotal Product Guide,
(b) this EULA and (c) the order.

12.12   Entire Agreement. This EULA (i) is the complete statement of the
agreement of the parties with regard to the subject matter hereof; and (ii)
may be modified only by a writing signed by both parties. All terms of any
purchase order or similar document provided by Customer, including but not
limited to any pre-printed terms thereon and any terms that are
inconsistent or conflict with this EULA, shall be null and void and of no
legal force or effect.

12.13   Contact Information. Please direct legal notices or other
correspondence to Pivotal Software, Inc., 3495 Deer Creek Road, Palo Alto,
CA 94304, United States of America, Attn: legal@gopivotal.com.

13.	COUNTRY SPECIFIC TERMS [IRELAND]. The terms in this Section 13 apply
only when Pivotal means the Pivotal sales subsidiary located in Ireland
(currently GoPivotal International Limited) and for the avoidance of doubt
these terms below shall replace the terms in the EULA above as specifically
stated and all other terms shall remain unchanged:

13.1  Section 9.2 (WARRANTY EXCLUSIONS). The entire section is deleted and
replaced with: 
	9.2	WARRANTY EXCLUSIONS. EXCEPT AS EXPRESSLY STATED IN THE
	APPLICABLE WARRANTY SET FORTH IN THIS EULA (INCLUDING ITS SUPPLIERS)
	MAKES NO OTHER EXPRESS OR IMPLIED WARRANTIES, WRITTEN OR ORAL. INSOFAR
	AS PERMITTED UNDER APPLICABLE LAW, ALL OTHER WARRANTIES ARE
	SPECIFICALLY EXCLUDED, INCLUDING WARRANTIES ARISING BY STATUTE, COURSE
	OF DEALING OR USAGE OF TRADE.

13.2  Section 9.3 (LIMITATION OF LIABILITY). The entire section is deleted
and replaced with the: 
9.3	LIMITATION OF LIABILITY. 
	A.	In case of death or personal injury caused by Pivotal’s
	negligence, in case of Pivotal’s willful  misconduct, fraud or gross
	negligence, and where a limitation of liability is not permissible
	under applicable mandatory law, Pivotal shall be liable according to
	statutory law. 
	B. 	Subject always to subsection 9.3.A, the liability of Pivotal
	(including its suppliers) to the Customer under or in connection with a
	Customer’s purchase order, whether arising from negligent error or
	omission, breach of contract, or otherwise ("Defaults") shall not exceed
	the (i) one million euros (€1,000,000) for damage to real or tangible
	personal property; and (ii) the price paid by Customer to Pivotal for the
	specific service (calculated on an annual basis, when applicable) or
	Software from which such claim arise, for damages of any type not
	identified in (i) above or otherwise excluded hereunder. 
	C.  In no event shall Pivotal (including its suppliers) be liable to
	Customer however that liability arises, for the following losses,
	whether direct, consequential, special, incidental, punitive or
	indirect: (i) loss of actual or anticipated revenue or profits, loss of
	use, loss of actual or anticipated savings, loss of or breach of
	contracts, loss of goodwill or reputation, loss of business
	opportunity, loss of business, wasted management time, cost of
	substitute services or facilities, loss of use of any software or data;
	and/or (ii) indirect, consequential, exemplary or incidental or special
	loss or damage; and/or (iii) damages, costs and/or expenses due to
	third party claims; and/or (iv) loss or damage due to the Customer’s
	failure to comply with obligations under this EULA, failure to do
	back-ups of data or any other matter under the control of the Customer
	and in each case whether or not any such losses were direct, foreseen,
	foreseeable, known or otherwise, and whether or not that party was
	aware of the circumstances in which such losses could arise. For the
	purposes of this Section 9.3, the term "loss" shall include a partial
	loss, as well as a complete or total loss. 
	D.  The parties expressly agree that should any limitation or provision
	contained in this Section 9.3 be held to be invalid under any applicable
	statute or rule of law, it shall to that extent be deemed omitted, but if
	any party thereby becomes liable for loss or damage which would otherwise
	have been excluded such liability shall be subject to the other limitations
	and provisions set out in this Section 9.3. 
	E.  The parties expressly agree that any order for specific performance made
	in connection with this EULA in respect of Pivotal shall be subject to the
	financial limitations set out in sub-section 9.3.B. 
	F.  CUSTOMER OBLIGATIONS IN RESPECT OF PRESERVATION OF DATA. During the
	Term of the EULA the Customer shall: 1)	from a point in time prior to
	the point of failure, (i) make full and/or incremental backups of data
	which allow recovery in an application consistent form, and (ii) store such
	back-ups at an off-site location sufficiently distant to avoid being
	impacted by the event(s) (e.g. including but not limited to flood, fire,
	power loss, denial of access or air crash) and affect the availability of
	data at the impacted site; 2) have adequate processes and procedures in
	place to restore data back to a point in time and prior to point of
	failure, and in the event of real or perceived data loss, provide the
	skills/backup and outage windows to restore the data in question; 3) use
	anti-virus software, regularly install updates across all data which is
	accessible across the network, and protect all storage arrays against power
	surges and unplanned power outages with uninterruptible power supplies; and
	4) ensure that all operating system, firmware, system utility (e.g. but
	not limited to, volume management, cluster management and backup) and patch
	levels are kept to Pivotal recommended versions and that any proposed
	changes thereto shall be communicated to Pivotal in a timely fashion.

13.3 Section 9.4 (Limitation Period). The entire section is deleted and
replaced with: 
	9.4  WAIVER OF RIGHT TO BRING ACTIONS. Customer waives the right to bring
	any claim arising out of or in connection with this EULA more than
	twenty-four (24) months after the date of the cause of  action giving rise
	to such claim.

Rev: Pivotal_GPDB_EULA_03182014.txt


I HAVE READ AND AGREE TO THE TERMS OF THE ABOVE PIVOTAL SOFTWARE
LICENSE AGREEMENT.

EOF

agreed=
while [ -z "${agreed}" ] ; do
    cat << EOF

********************************************************************************
Do you accept the Pivotal Database license agreement? [yes|no]
********************************************************************************

EOF
    read reply leftover
        case $reply in
           [yY] | [yY][eE][sS])
                agreed=1
                ;;
           [nN] | [nN][oO])
                cat << EOF

********************************************************************************
You must accept the license agreement in order to install Greenplum Database
********************************************************************************
                             
                   **************************************** 
                   *          Exiting installer           *
                   **************************************** 

EOF
                exit 1
                ;;
        esac
done

installPath=/usr/local/greenplum-db-%%GP_VERSION%%
defaultinstallPath=${installPath}
user_specified_installPath=

while [ -z "${user_specified_installPath}" ] ; do
	cat <<-EOF
	
		********************************************************************************
		Provide the installation path for Greenplum Database or press ENTER to 
		accept the default installation path: $defaultinstallPath
		********************************************************************************
	
	EOF

    read user_specified_installPath leftover

    if [ -z "${user_specified_installPath}" ] ; then
        user_specified_installPath=${installPath}
    fi

    if [ -n "${leftover}" ] ; then
	    cat <<-EOF
			
			********************************************************************************
			WARNING: Spaces are not allowed in the installation path.  Please specify
			         an installation path without an embedded space.
			********************************************************************************
			
		EOF
        user_specified_installPath=
        continue
    fi

    pathVerification=
	while [ -z "${pathVerification}" ] ; do
	    cat <<-EOF
			
			********************************************************************************
			Install Greenplum Database into ${user_specified_installPath}? [yes|no]
			********************************************************************************
			
		EOF
	
	    read pathVerification leftover
	
	    case $pathVerification in
	        [yY] | [yY][eE][sS])
	            pathVerification=1
                installPath=${user_specified_installPath}
	            ;;
	        [nN] | [nN][oO])
	            user_specified_installPath=
	           ;;
	    esac
	done
done

if [ ! -d "${installPath}" ] ; then
    agreed=
    while [ -z "${agreed}" ] ; do
    cat << EOF

********************************************************************************
${installPath} does not exist.
Create ${installPath} ? [yes|no]
(Selecting no will exit the installer)
********************************************************************************

EOF
    read reply leftover
        case $reply in
           [yY] | [yY][eE][sS])
                agreed=1
                ;;
           [nN] | [nN][oO])
                cat << EOF

********************************************************************************
                             Exiting the installer
********************************************************************************

EOF
                exit 1
                ;;
        esac
    done
    mkdir -p ${installPath}
fi

if [ ! -w "${installPath}" ] ; then
    echo "${installPath} does not appear to be writeable for your user account."
    echo "Continue?"
    continue=
    while [ -z "${continue}" ] ; do
        read continue leftover
            case ${continue} in
                [yY] | [yY][eE][sS])
                    continue=1
                    ;;
                [nN] | [nN][oO])
                    echo "Exiting Greenplum Database installation."
                    exit 1
                    ;;
            esac
    done
fi

if [ ! -d ${installPath} ] ; then
    echo "Creating ${installPath}"
    mkdir -p ${installPath}
    if [ $? -ne "0" ] ; then
        echo "Error creating ${installPath}"
        exit 1
    fi
fi 


echo ""
echo "Extracting product to ${installPath}"
echo ""
tail -n +${SKIP} "$0" | ${TAR} zxf - -C ${installPath}
if [ $? -ne 0 ] ; then
    cat <<-EOF
********************************************************************************
********************************************************************************
                          Error in extracting Greenplum Database
                               Installation failed
********************************************************************************
********************************************************************************

EOF
    exit 1
fi

installDir=`basename ${installPath}`
symlinkPath=`dirname ${installPath}`
symlinkLink=greenplum-db
if [ x"${symlinkLink}" != x"${installDir}" ]; then
    if [ "`ls ${symlinkPath}/${symlinkLink} 2> /dev/null`" = "" ]; then
        ln -s "./${installDir}" "${symlinkPath}/${symlinkLink}"
    fi
fi
sed "s,^GPHOME.*,GPHOME=${installPath}," ${installPath}/greenplum_path.sh > ${installPath}/greenplum_path.sh.tmp
mv ${installPath}/greenplum_path.sh.tmp ${installPath}/greenplum_path.sh

    cat <<-EOF
********************************************************************************
Installation complete.
Greenplum Database is installed in ${installPath}

Pivotal Greenplum documentation is available
for download at http://gpdb.docs.pivotal.io
********************************************************************************
EOF

exit 0

__END_HEADER__
