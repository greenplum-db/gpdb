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
You must read and accept the Pivotal End User license agreement
before installing
********************************************************************************

END USER LICENSE AGREEMENT


BACKGROUND. This End User License Agreement ("Agreement") is between Pivotal Software, Inc. (or based on Licensee's location (i) the local Pivotal sales subsidiary, if Licensee is located in a country outside the United States in which Pivotal has a local sales subsidiary; or (ii) Pivotal Software International (subject to Section 12 Country Specific Terms (International)), if Licensee is located in a country outside the United States in which Pivotal does not have a local sales subsidiary) in each case, "Pivotal" and Licensee.


This Agreement governs Licensee's procurement and use of all Software and Support Services ordered by Licensee directly from Pivotal or its Distributor. Pivotal shall provide the Software and Support Services as described in each Quote or Order referencing this Agreement. Unless otherwise set forth in a separate signed agreement between Pivotal or its Distributor and Licensee, by downloading, installing, or using the Software. Licensee agrees to these terms.


1. EVALUATION SOFTWARE AND BETA COMPONENTS. If Licensee licenses Evaluation Software, Beta Components, or both, then such Evaluation Software and Beta Components are licensed by Pivotal to Licensee on a non-exclusive, non-transferable basis, without any right to sublicense, up to the maximum licensed capacity during the Evaluation Period, in the Territory, subject to the Guide, only for Licensee's internal business operations in a non-production environment. Notwithstanding any other provision in this Agreement, Evaluation Software and Beta Components are provided "AS-IS" without indemnification, support, or warranty of any kind, expressed or implied. All such licenses expire at the end of the Evaluation Period.
2. GRANT AND USE RIGHTS FOR SOFTWARE.
   1. License Grant. The Software is licensed, not sold. Nothing in this Agreement shall be construed to mean that Pivotal has sold or otherwise transferred ownership of the Software. Pivotal grants Licensee a non-exclusive, non-transferable license, without any right to sublicense, to use the Software, Documentation and related Support Services, up to the maximum licensed capacity during the period identified in the Quote, in the Territory, subject to the Guide, only for Licensee's internal business operations. Should Licensee exceed the Software's licensed capacity, Licensee will promptly procure additional Software license rights at a mutually agreed price. Third Party Agents may access the Software on Licensee's behalf during the Subscription Period solely for Licensee's internal business operations. Licensee may make one unmodified backup copy of the Software solely for archival purposes. If Licensee upgrades or exchanges the Software from a previous validly licensed version, Licensee must cease using all prior versions of the Software and certify cessation of use to Pivotal. Licensee is responsible for obtaining any software, hardware, or other technology required to operate the Software and complying with any corresponding terms and conditions.
   2. License Restrictions. Licensee must not, and must not allow any third party to: (a) use the Software in an application services provider, service bureau, or similar capacity; (b) disclose to any third party the results of any benchmark testing or comparative or competitive analyses of the Software without Pivotal's prior written approval; (c) make the Software available for access or use to any third party except as otherwise expressly permitted by Pivotal; (d) transfer or sublicense the Software or Documentation (other than to an Affiliate, subject to Pivotal's prior written approval); (e) use the Software in conflict with the Guide, Quote or Order; (f)  modify, translate, enhance, or create derivative works from the Software, or reverse assemble or disassemble, reverse engineer, decompile, or otherwise attempt to derive source code from Software except as permitted by applicable mandatory law or third party license; (g) remove any copyright or other proprietary notices on or in the Software; or (h) violate or circumvent any technological restrictions within the Software or as otherwise specified in this Agreement.
   3. OSS. OSS is licensed to Licensee under the applicable OSS license terms located in the open_source_licenses.txt file included in or along with the Software, the Evaluation Software, or the corresponding source files available at https://network.pivotal.io/open-source. The applicable OSS license terms are also available by sending a written request, with Licensee's name and address, to: Pivotal Software, Inc., Open Source Files Request, Attn: General Counsel, 875 Howard Street, 5th Floor, San Francisco, CA 94103. This offer to obtain a copy of the licenses and source files is valid for three years from the date Licensee first acquired access to the Software. OSS terms and conditions shall take precedence over this Agreement solely with respect to such OSS.
   4. Subscription License. All Subscription Licenses are subject to a non-cancelable and non-refundable fee. If a Quote or Order indicates a Subscription License, then the terms in this Section 2.4 (Subscription License) shall also apply. At least 60 days before expiration of the Subscription Period, Pivotal (or Distributor, if applicable) will notify Licensee of its option to renew the Subscription License at the end of the Subscription Period for one additional year at the same annual rate stated in the Quote or Order, plus 5%. If Licensee does not notify Pivotal (or Distributor, if applicable) at least thirty days before expiration of the Subscription Period of Licensee's intent to renew, the Subscription License shall expire at the end of the Subscription Period. Licensee agrees to cease using the Software at the expiration of the Subscription Period and any renewal period and will certify cessation of use to Pivotal.
   5. Decompilation. If applicable laws in the Territory grant an express right to decompile the Software to render it interoperable with other software, Licensee may decompile the Software, but must first request Pivotal to do so. Licensee must provide all requested information to allow Pivotal to assess the request. Pivotal may, in its discretion, provide such interoperability information, impose reasonable conditions, including a reasonable fee, on such use of the Software, or offer to provide alternatives to protect Pivotal's proprietary rights.
1. ORDERS. Licensee's Order is subject to this Agreement and shall reference the applicable Quote. No Orders are binding until accepted by Pivotal (or Distributor, if applicable). Orders for Software are deemed accepted upon Pivotal's (or Distributor's, if applicable) delivery of Software included in such Order. Orders issued to Pivotal do not have to be signed to be valid and enforceable. Licensee shall pay in full in accordance with Pivotal's invoice (or Distributor's invoice, if applicable).
2. LIMITED WARRANTY.
   1. Software Warranty. Pivotal warrants to Licensee that the Software will, for the Warranty Period, substantially conform to the applicable Documentation, provided the Software: (a) has been properly installed and used in accordance with the Documentation; and (b) has not been modified by persons other than Pivotal. For any breach of this warranty, Pivotal will, at its option and expense, and as Licensee's exclusive remedy, either replace the Software or correct any reproducible error in the Software reported to Pivotal by Licensee in writing during the Warranty Period. If Pivotal determines that it is unable to replace the Software or correct the error, Pivotal will refund to Licensee (or Distributor, if applicable) the amount paid by Licensee (or Distributor, if applicable) for the Software, and the license will terminate.
   2. Warranty Exclusions. EXCEPT AS SET FORTH IN SECTION 4.1, AND TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, PIVOTAL AND ITS DISTRIBUTORS PROVIDE THE SOFTWARE AND THE SUPPORT SERVICES WITHOUT ANY WARRANTIES OF ANY KIND, EXPRESS, IMPLIED, STATUTORY, OR IN ANY OTHER PROVISION OF THIS AGREEMENT OR COMMUNICATION WITH LICENSEE, AND PIVOTAL AND ITS DISTRIBUTORS SPECIFICALLY DISCLAIM ANY IMPLIED WARRANTIES OR CONDITIONS OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT, TITLE, AND ANY WARRANTIES ARISING FROM COURSE OF DEALING OR COURSE OF PERFORMANCE REGARDING OR RELATING TO THE SOFTWARE, THE SUPPORT SERVICES, THE DOCUMENTATION, OR ANY MATERIALS FURNISHED OR PROVIDED TO LICENSEE UNDER THIS AGREEMENT. PIVOTAL AND ITS DISTRIBUTORS DO NOT WARRANT THAT THE SOFTWARE WILL OPERATE UNINTERRUPTED, OR THAT IT WILL BE FREE FROM DEFECTS OR THAT THE SOFTWARE WILL MEET (OR IS DESIGNED TO MEET) LICENSEE'S BUSINESS REQUIREMENTS.
1. IP INDEMNITY.
   1. IP Indemnity for Software. Subject to the remainder of this Section 5 (IP Indemnity) and Section 6 (Limitation of Liability), Pivotal shall: (a) defend Licensee against any Claim that the Software infringes a copyright or patent enforceable in a Berne Convention signatory country; and (b) pay resulting costs and damages finally awarded against Licensee by a court of competent jurisdiction, or pay amounts stated in a written settlement negotiated and approved by Pivotal.
   2. Procedure and Remedies. The foregoing obligations apply only if Licensee: (a) promptly notifies Pivotal in writing of such Claim; (b) grants Pivotal sole control over the defense and settlement of such Claim; (c) reasonably cooperates in response to Pivotal's request for assistance; (d) is not in material breach of this Agreement; and (e) is current in payment of all applicable fees prior to the Claim. If the allegedly infringing Software is held to constitute an infringement, or in Pivotal's opinion, any such Software is likely to become infringing and its use enjoined, Pivotal may, at its sole option and expense: (i) procure for Licensee the right to make continued use of the affected Software; (ii) replace or modify the affected Software to make it non-infringing; or (iii) notify Licensee to return the affected Software and, upon receipt, discontinue the related Support Services (if applicable) and, for Subscription Licenses, refund unused prepaid fees calculated based on each month remaining in the Subscription Period.
   3. IP Indemnity Exclusions. Neither Pivotal nor any Distributor shall have any obligation under this Section 5 (IP Indemnity) or otherwise with respect to any Claim that arises out of or relates to: (a) combination, operation or use of the Software with any other software, hardware, technology, data, or other materials; (b) use for a purpose or in a manner for which the Software was not designed or use after Pivotal notifies Licensee to cease such use due to a possible or pending Claim; (c) any modifications to the Software made by any person other than Pivotal or its authorized representatives; (d) any modifications to the Software made by Pivotal pursuant to instructions, designs, specifications, or any other information or materials provided to Pivotal by or on behalf of Licensee; (e) use of any version of the Software when an upgrade or a newer iteration of the Software made available by Pivotal could have avoided the infringement; (f) any data or information which Licensee or a third party utilizes in connection with the Software; or (g) any Open Source Software. THIS SECTION 5 STATES LICENSEE'S SOLE AND EXCLUSIVE REMEDY AND PIVOTAL'S ENTIRE LIABILITY FOR ANY INFRINGEMENT CLAIMS.
1. LIMITATION OF LIABILITY. TO THE MAXIMUM EXTENT MANDATED BY LAW, IN NO EVENT SHALL PIVOTAL OR ITS DISTRIBUTORS BE LIABLE FOR ANY LOST PROFITS OR BUSINESS OPPORTUNITIES, LOSS OF USE, LOSS OF REVENUE, LOSS OF GOODWILL, BUSINESS INTERRUPTION, LOSS OF DATA, OR ANY OTHER INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES UNDER ANY THEORY OF LIABILITY, WHETHER BASED IN CONTRACT, TORT, NEGLIGENCE, PRODUCT LIABILITY OR OTHERWISE. PIVOTAL'S AND ITS DISTRIBUTORS' LIABILITY UNDER THIS AGREEMENT SHALL NOT, IN ANY EVENT, EXCEED THE LESSER OF (A) FEES LICENSEE PAID FOR THE SOFTWARE DURING THE 12 MONTHS PRECEDING THE DATE PIVOTAL RECEIVES WRITTEN NOTICE OF THE FIRST CLAIM TO ARISE UNDER THIS AGREEMENT; OR (B) USD $1,000,000. THE FOREGOING LIMITATIONS SHALL APPLY REGARDLESS OF WHETHER PIVOTAL OR ITS DISTRIBUTORS HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES AND REGARDLESS OF WHETHER ANY REMEDY FAILS OF ITS ESSENTIAL PURPOSE. LICENSEE MAY NOT BRING A CLAIM UNDER THIS AGREEMENT MORE THAN 18 MONTHS AFTER (i) THE END OF THE SUBSCRIPTION PERIOD, FOR SUBSCRIPTION LICENSES, AND (ii) THE CLAIM FIRST ARISES FOR ALL OTHER CLAIMS.
2. TERMINATION.
   1. For Cause. Pivotal may terminate this Agreement effective immediately upon written notice to Licensee if: (a) Licensee fails to pay any portion of fees due under an applicable Quote or Order within ten days after receiving notice that payment is past due; (b) Licensee suffers an insolvency or analogous event; (c) Licensee commits a material breach of this Agreement that is incapable of being cured; or (d) Licensee breaches any other provision of this Agreement and does not cure the breach within 30 days after receiving written notice of breach.
   2. For Convenience. Pivotal may terminate this Agreement for convenience upon three months written notice to the Licensee.
   3. Effect of Termination. In the event of expiration of a Subscription License or any termination of this Agreement, Licensee must remove and destroy all copies of Software, including all backup copies, from the server, virtual machine, and all computers and terminals on which the Software (including copies) is installed or used and certify destruction of the Software. All provisions of this Agreement will survive any termination or expiration if by its nature and context it is intended to survive.
1. CONFIDENTIALITY. Each party shall: (a) use the other party's Confidential Information only for exercising rights and performing obligations in connection with this Agreement; and (b) protect from disclosure any Confidential Information disclosed by the other party for a period commencing upon the disclosure date until three years later. Notwithstanding the foregoing, either party may disclose Confidential Information: (i) to an Affiliate to fulfill its obligations or exercise its rights under this Agreement so long as such Affiliate agrees to comply with these restrictions in writing; and (ii) if required by law or regulatory authorities provided the receiving party has given the disclosing party prompt notice before disclosure. Pivotal shall not be responsible for unauthorized disclosure of Licensee's data stored within Software arising from a data security breach. Licensee is solely responsible for all obligations to comply with laws applicable to Licensee's Software use, including without limitation any personal data processing. Pivotal may collect, use, store and transmit technical and related information about Licensee's Software use, including server internet protocol addresses, hardware identification, operating system, application software, peripheral hardware, and Software usage statistics, to facilitate the provisioning of Support Services. Licensee is responsible for obtaining all consents required to enable Pivotal to exercise its confidentiality rights, in compliance with applicable law.
2.  RECORDS/AUDIT. For the period set forth in the Quote or Order, any renewals, and for three years after, Licensee shall maintain accurate records regarding its compliance with this Agreement. Upon reasonable notice and not more than once per year, Pivotal may audit Licensee's Software use to determine such compliance and payment of fees. Licensee will promptly pay additional fees identified by the audit and reimburse Pivotal for all audit costs if the audit discloses underpayment by more than 5% in the audited period or that Licensee breached any Agreement term.
3. FEEDBACK AND RESERVATION OF IP RIGHTS.
   1. Feedback. The parties agree that any feedback or suggestions ("Feedback") (if any) given is voluntary. Each party is free to use, disclose, reproduce, license or otherwise distribute the Feedback relating to its own products and services, without any obligations or restrictions of any kind, including intellectual property rights.
   2. Reservation of IP Rights. Except as expressly stated, nothing in this Agreement shall be construed to: (a) directly or indirectly grant to a receiving party any title or license to or ownership of a providing party's intellectual property rights in the Software, Support Services, or materials furnished by such providing party; or (b) preclude such providing party from: (i) independently developing, marketing, acquiring, using, licensing, modifying or otherwise freely exploiting products or services that are similar to or related to the Software or materials provided under this Agreement; (ii) restricting the assignment of persons performing Support Services; or (iii) using and employing their general skills, know-how, and expertise, and to use, disclose, and employ any generalized ideas, concepts, know-how, methods, techniques, or skills gained or learned during the course of any assignment, so long as that party complies with confidentiality obligations in this Agreement. Pivotal is not being engaged to perform any investigation of third party intellectual property rights including any searches of patents, copyrights, or trademarks related to the Software.
1. EXPORT AND TRADE COMPLIANCE. The Software and any technology delivered in connection with them pursuant to this Agreement may be subject to governmental restrictions on exports from the USA, restrictions on exports from other countries in which such Software and technology may be provided or located, disclosures of technology to foreign persons, exports from abroad of derivative products, and the importation or use of such technology included with them outside of the USA (collectively, "Export Laws"). Diversion contrary to Export Laws is expressly prohibited. Licensee shall, at its sole expense, comply with all Export Laws including without limitation all licensing, authorization, documentation and reporting requirements and Pivotal export policies made available to Licensee by Pivotal. Licensee represents that it is not a Restricted Party, which shall be deemed to include any person or entity: (a) located in or a national of Cuba, Iran, North Korea, Sudan, Syria, Crimea, or any other countries that may, from time to time, become sanctioned or with which U.S. persons are generally prohibited from engaging in financial transactions; (b) on any restricted party or entity list maintained by any U.S. government agency; or (c) any person or entity involved in an activity restricted by any U.S. government agency. Certain information or technology may be subject to the International Traffic in Arms Regulations and shall only be exported, transferred or released to foreign nationals inside or outside the United States in compliance with such regulations.
2. GENERAL. This Agreement is governed and interpreted by California law. Any lawsuit arising directly or indirectly out of this Agreement shall be litigated in the Superior Court of San Francisco, California or, if original jurisdiction can be established, in the United States District Court for the Northern District of California. The U.N. Convention on Contracts for the International Sale of Goods does not apply. Software and Support Services are subject to United States, European Union, and other export and import laws and regulations. Both parties shall comply with all applicable laws and regulations and diversion contrary to such laws is expressly prohibited. This Agreement confers no rights or remedies on any third party, other than the parties to this Agreement and their respective successors and permitted assigns. Pivotal reserves all rights not expressly granted to Licensee in this Agreement. The parties are independent contractors. This Agreement and its attachments contain the entire understanding between the parties and may be amended only by a written document signed by both parties. Licensee shall not assign or transfer any rights under this Agreement or delegate any of its duties under this Agreement without Pivotal's prior written consent, and any such action in violation of this provision, is null and void, of no force, and a breach of this Agreement. Pivotal may assign or transfer this Agreement to any successors-in-interest to all or substantially all of the business or assets of Pivotal whether by merger, reorganization, asset sale or otherwise, or to any Affiliates of Pivotal, and this Agreement shall inure to the benefit of and be binding upon the respective permitted successors and assigns. Pivotal may use Pivotal Affiliates or other sufficiently qualified subcontractors to provide Support Services, provided that Pivotal remains responsible for their performance. If any part of this Agreement, an Order, or a Quote is held unenforceable, the validity of the remaining provisions shall not be affected. In the event of conflict or inconsistency among the Guide, this Agreement and the Order or Quote, the following order of precedence shall apply: (a) the Guide, (b) this Agreement and (c) the Order. All terms of any Licensee purchase order or similar document provided by Licensee, except those confirming the business terms set forth in the applicable Pivotal Quote, shall be null and void and of no legal force or effect, even if Pivotal does not expressly reject such terms when accepting a purchase order or similar document provided by Licensee.
3. COUNTRY SPECIFIC TERMS (INTERNATIONAL). The terms in this Section 12 (Country Specific Terms (International)) only apply when Pivotal means Pivotal Software International. For the avoidance of doubt the terms of this Section 12 (Country Specific Terms (International)) shall replace the terms in the Agreement as specifically stated and all other terms of the Agreement shall remain unchanged.
   1. Section 4 (LIMITED WARRANTY). Section 4.2 (Warranty Exclusions) shall be deleted and replaced with: 4.2 Warranty Exclusions. EXCEPT AS EXPRESSLY STATED IN THE APPLICABLE WARRANTY SET FORTH IN THIS AGREEMENT, PIVOTAL (INCLUDING ITS SUPPLIERS) MAKES NO OTHER EXPRESS OR IMPLIED WARRANTIES, WRITTEN OR ORAL. INSOFAR AS PERMITTED UNDER APPLICABLE LAW, ALL OTHER WARRANTIES ARE SPECIFICALLY EXCLUDED, INCLUDING WARRANTIES ARISING BY STATUTE, COURSE OF DEALING, OR USAGE OF TRADE.
   2. Section 6 (LIMITATION OF LIABILITY). Section 6 (Limitation of Liability) shall be deleted replaced with: 6. LIMITATION OF LIABILITY.
1. In case of death or personal injury caused by Pivotal's negligence, in case of Pivotal's willful misconduct, fraud, or gross negligence, and where a limitation of liability is not permissible under applicable mandatory law, Pivotal shall be liable according to statutory law.
2. Subject always to sub-section 6.A, the liability of Pivotal (including its suppliers) to the Licensee under or in connection with a Licensee's Order, whether arising from negligent error or omission, breach of contract, or otherwise shall not exceed the lesser of (i) fees Licensee paid for the specific service (calculated on an annual basis, when applicable) or Software during the 12 months preceding Pivotal's notice of such claim; or (ii) EUR EUR1,000,000.
3. In no event shall Pivotal (including its suppliers) be liable to Licensee however that liability arises, for the following losses, whether direct, consequential, special, incidental, punitive or indirect: (i) loss of actual or anticipated revenue or profits, loss of use, loss of actual or anticipated savings, loss of or breach of contracts, loss of goodwill or reputation, loss of business opportunity, loss of business, wasted management time, cost of substitute services or facilities, loss of use of any software or data; and/or (ii) indirect, consequential, exemplary or incidental or special loss or damage; and/or (iii) damages, costs and/or expenses due to third party claims; and/or (iv) loss or damage due to the Licensee's failure to comply with obligations under this Agreement, failure to do back-ups of data or any other matter under the control of the Licensee and in each case whether or not any such losses were direct, foreseen, foreseeable, known or otherwise, and whether or not that party was aware of the circumstances in which such losses could arise. For the purposes of this Section 6 (Limitation of Liability), the term "loss" shall include a partial loss, as well as a complete or total loss.
4. The parties expressly agree that should any limitation or provision contained in this Section 6 (Limitation of Liability) be held to be invalid under any applicable statute or rule of law, it shall to that extent be deemed omitted, but if any party thereby becomes liable for loss or damage which would otherwise have been excluded such liability shall be subject to the other limitations and provisions set out in this Section 6 (Limitation of Liability).
5. The parties expressly agree that any order for specific performance made in connection with this Agreement in respect of Pivotal shall be subject to the financial limitations set out in sub-section 6.B.
6. Licensee waives the right to bring any claim arising out of or in connection with this Agreement more than twenty-four months after the date of the cause of action giving rise to such claim.
7. LICENSEE OBLIGATIONS IN RESPECT OF PRESERVATION OF DATA. During the term of the EULA the Licensee shall:
   1. from a point in time prior to the point of failure, (i) make full and/or incremental backups of data which allow recovery in an application consistent form, and (ii) store such back-ups at an off-site location sufficiently distant to avoid being impacted by the event(s) (e.g. including but not limited to flood, fire, power loss, denial of access or air crash) and affect the availability of data at the impacted site;
   2. have adequate processes and procedures in place to restore data back to a point in time and prior to point of failure, and in the event of real or perceived data loss, provide the skills/backup and outage windows to restore the data in question;
   3. use anti-virus software, regularly install updates across all data which is accessible across the network, and protect all storage arrays against power surges and unplanned power outages with uninterruptible power supplies; and
   4. ensure that all operating system, firmware, system utility (e.g. but not limited to, volume management, cluster management and backup) and patch levels are kept to Pivotal recommended versions and that any proposed changes thereto shall be communicated to Pivotal in a timely fashion.
   1. Section 11 (GENERAL) The first two sentences of Section 11 (General) shall be deleted and replaced with: This Agreement is governed by the laws of the Republic of Ireland, excluding its conflict of law rules. Each party expressly consents to the personal jurisdiction of the Dublin Courts and agrees that any lawsuit arising directly or indirectly out of this Agreement shall be litigated in the Dublin Courts.
1. DEFINITIONS
Affiliate means a legal entity controlled by, controlling, or that is under common control of Pivotal or Licensee, with control meaning more than 50% of the voting power or ownership interests then outstanding of that entity.
Beta Component means a Software component not yet generally available but included in the Software.
Claim means any third party claim, notice, demand, action, proceeding, litigation, investigation, or judgment. With respect to Software, such Claim must be related to Licensee's use of the Software during the Subscription Period or renewal period.
Confidential Information means the terms of this Agreement, the Software, and all confidential and proprietary information of Pivotal or Licensee, including without limitation, all business plans, product plans, financial information, software, designs, technical, business, or financial data of any nature whatsoever, provided that such information is marked or designated in writing as "confidential," "proprietary," or with a similar term or designation, or information that would reasonably be regarded as being confidential by its nature. Confidential Information excludes information that is: (a) rightfully in the receiving party's possession without prior obligation of confidentiality from the disclosing party; (b) a matter of public knowledge (or becomes a matter of public knowledge other than through a breach of confidentiality by the other party); (c) rightfully furnished to the receiving party by a third party without a confidentiality restriction; or (d) independently developed by the receiving party without reference to the disclosing party's Confidential Information.
Distributor means a reseller, distributor, system integrator, service provider, independent software vendor, value-added reseller or other partner authorized by Pivotal to license Software to end users, or any third party duly authorized by a Distributor to license Software to end users.
Documentation means documentation provided to Licensee by Pivotal with the Software, as revised by Pivotal from time to time.
Evaluation Period means 90 days starting from initial delivery of the Evaluation Software or Beta Components.
Evaluation Software means Software made available for the Evaluation Period at no charge, for Licensee's evaluation purposes only, either subject to a signed Order, or where Licensee has not signed a Quote.
Guide means the Pivotal Product Guide available at: https://www.pivotal.io/product-guide, in effect on the date of the Quote and incorporated into this Agreement.
Licensee means the person or the entity, and its permitted successors and assigns, obtaining the Software.
Major Release means a generally available release of Software that Pivotal designates with a change in the digit to the left of the first decimal point (e.g., 5.0 >> 6.0).
Minor Release means a generally available release of Software that Pivotal designated with a change in the digit to the right of the decimal point (e.g., 5.0 >> 5.1).
Open Source Software or OSS means software components licensed and distributed under a license approved by the Open Source Initiative or similar open source or freeware license and included in, embedded in, utilized by, or provided or distributed with the Software.
Order means a purchase order or other ordering document either signed by the parties or issued by Licensee to Pivotal or a Distributor that references and incorporates this Agreement and is accepted by Pivotal as set forth in Section 3 (Orders).
Perpetual License means access to Software and Documentation subject to the licensing terms and restrictions in the Guide on a perpetual basis.
Quote means a pricing quote issued by Pivotal or its Distributor.
Software means Pivotal computer programs listed in the Guide and identified in a Quote, indicating a Perpetual License or Subscription License.
Subscription License means a license during the Subscription Period to access: (a) Software and Documentation set forth in the Quote subject to the Guide; and (b) Support Services, which include any Major Releases, Minor Releases, or upgrades on a "when and if available" basis.
Subscription Period means the period specified in the Quote or Order beginning upon notification to Licensee that the Software is available for download.
Support Services means services described at: https://www.pivotal.io/support.
Territory means the country or countries in which Licensee has been invoiced.
Third Party Agent means Licensee's employees or contractors delivering information technology services to Licensee pursuant to a written contract requiring compliance with this Agreement.
Warranty Period means 90 days starting from the first notice of availability of the Software for download.


End User License Agreement - March 13, 2019

EOF

agreed=
while [ -z "${agreed}" ] ; do
    cat << EOF

********************************************************************************
Do you accept the Pivotal End User license agreement? [yes|no]
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
You must accept the license agreement in order to install Pivotal Greenplum
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
		Provide the installation path for Pivotal Greenplum or press ENTER to
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
			Install Pivotal Greenplum into ${user_specified_installPath}? [yes|no]
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
                    echo "Exiting Pivotal Greenplum installation."
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
                          Error in extracting Pivotal Greenplum
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
Pivotal Greenplum is installed in ${installPath}

Pivotal Greenplum documentation is available
for download at http://gpdb.docs.pivotal.io
********************************************************************************
EOF

exit 0

__END_HEADER__
