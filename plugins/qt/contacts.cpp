/**
 * @file contacts.cpp
 * @brief QContact phonebook
 */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is matd.
 *
 * The Initial Developer of the Original Code is
 * remi.denis-courmont@nokia.com.
 * Portions created by the Initial Developer are
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>

#include <QContactManager>
#include <QContact>
#include <QContactName>
#include <QContactPhoneNumber>
#include <QContactEmailAddress>

#include <at_command.h>
#include <at_thread.h>

QTM_USE_NAMESPACE

class QATPhonebook
{
	private:
		QContactManager *mgr;

		static at_error_t readCb(at_modem_t *, unsigned, unsigned, void *);
		at_error_t read(at_modem_t *, unsigned, unsigned);
		static at_error_t rangeCb(unsigned *, unsigned *, void *);
		at_error_t range(unsigned *, unsigned *);
		static at_error_t writeCb(at_modem_t *, unsigned *, const char *,
		                          const char *, const char *, const char *,
		                          const char *, const char *, const char *,
		                          const char *, bool, void *);
		at_error_t add(unsigned *, const QString&, const QString&,
		               const QString&, const QString&, const QString&,
		               const QString&, const QString&, const QString&);
		at_error_t edit(unsigned, const QString&, const QString&,
		                const QString&, const QString&, const QString&,
		                const QString&, const QString&, const QString&);
		at_error_t remove(unsigned);

	public:
		QATPhonebook(const QString& name = QString());
		int registerPhonebook(at_commands_t *set, const char *id);
};

QATPhonebook::QATPhonebook(const QString& name)
{
	this->mgr = new QContactManager (name);
}

at_error_t QATPhonebook::read(at_modem_t *m, unsigned start, unsigned end)
{
	QList<QContactLocalId> list = mgr->contactIds();

	end++;
	if (end > list.count())
		end = list.count();
	if (start >= end)
		return AT_OK;

	for (unsigned i = start; i < end; i++)
	{
		QContactLocalId id = list.at(i);
		QContact contact = mgr->contact(id);

		QString name = contact.displayLabel();

		QList<QContactDetail> numbers = contact.details(
			QContactPhoneNumber::DefinitionName);
		QString ph, ph2;
		switch (numbers.count())
		{
			case 0:
				break;
			case 1:
				ph = ((QContactPhoneNumber)numbers.value(0)).number();
			default:
				ph2 = ((QContactPhoneNumber)numbers.value(1)).number();
		}

		QList<QContactDetail> addresses = contact.details(
			QContactEmailAddress::DefinitionName);
		QString email;
		if (numbers.count() > 0)
			email = ((QContactEmailAddress)addresses.value(0)).emailAddress();

		at_intermediate (m, "\r\n+CPBR: %u,\"%s\",%u,\"%s\",0,\"\",\"%s\",%u,"
		                 "\"\",\"%s\"", i,
		                 ph.toUtf8().constData(), (ph[0] == '+') ? 145 : 129,
		                 name.toUtf8().constData(),
		                 ph2.toUtf8().constData(), (ph2[0] == '+') ? 145 : 129,
		                 email.toUtf8().constData());
	}
	return AT_OK;
}

at_error_t QATPhonebook::range(unsigned *startp, unsigned *endp)
{
	QList<QContactLocalId> list = mgr->contactIds();
	unsigned count = list.count();

	if (count == 0)
		return AT_CME_ENOENT;
	*startp = 0;
	*endp = count - 1;
	return AT_OK;
}

at_error_t QATPhonebook::add(unsigned *idxp, const QString& phone,
                             const QString& txt, const QString& grp,
                             const QString& phone2, const QString& txt2,
                             const QString& email, const QString& sip,
                             const QString& tel)
{
	QContact contact;
	QContactName name;
	QContactPhoneNumber num, num2;
	QContactEmailAddress address;

	name.setCustomLabel(txt);
	contact.saveDetail(&name);

	num.setContexts(QContactDetail::ContextHome);
	num.setSubTypes(QContactPhoneNumber::SubTypeMobile);
	num.setNumber(phone);
	contact.saveDetail(&num);

	num2.setContexts(QContactDetail::ContextWork);
	num2.setSubTypes(QContactPhoneNumber::SubTypeMobile);
	num2.setNumber(phone2);
	contact.saveDetail(&num2);

	address.setEmailAddress(email);
	contact.saveDetail(&address);

	if (!mgr->saveContact(&contact))
		return AT_CME_UNKNOWN;

	QList<QContactLocalId> list = mgr->contactIds();
	unsigned count = list.count();

	if (count == 0)
		return AT_CME_ENOENT;

	*idxp = list.indexOf(contact.localId());
	return AT_OK;
}

at_error_t QATPhonebook::edit(unsigned idx, const QString& num,
                              const QString& txt, const QString& grp,
                              const QString& num2, const QString& txt2,
                              const QString& email, const QString& sip,
                              const QString& tel)
{
	return AT_CME_ENOTSUP;
}

at_error_t QATPhonebook::remove(unsigned idx)
{
	QList<QContactLocalId> list = mgr->contactIds();

	if (idx >= list.count())
		return AT_CME_ENOENT;

	return mgr->removeContact(list.at(idx)) ? AT_OK : AT_CME_UNKNOWN;
}

/*** AT command callbacks ***/

at_error_t QATPhonebook::readCb(at_modem_t *m, unsigned start, unsigned end,
                                void *data)
{
	at_cancel_disabler disabler;
	QATPhonebook *pb = static_cast<QATPhonebook *>(data);

	return pb->read(m, start, end);
}

at_error_t QATPhonebook::rangeCb(unsigned *startp, unsigned *endp, void *data)
{
	at_cancel_disabler disabler;
	QATPhonebook *pb = static_cast<QATPhonebook *>(data);

	return pb->range(startp, endp);
}

at_error_t QATPhonebook::writeCb(at_modem_t *m, unsigned *idx,
	const char *num, const char *txt, const char *group,
	const char *num2, const char *txt2, const char *email,
	const char *sip, const char *tel, bool hidden, void *data)
{
	at_error_t ret;
	at_cancel_disabler disabler;
	QATPhonebook *pb = static_cast<QATPhonebook *>(data);

	if (hidden)
		return AT_CME_ENOTSUP;

	if (!*num && !*txt && !*group && !*num2 && !*txt2 &&!*email
	 && !*sip && !*tel)
		ret = pb->remove(*idx);
	else
	if (*idx == UINT_MAX)
	{

		ret = pb->add(idx, QString::fromUtf8(num), QString::fromUtf8(txt),
		              QString::fromUtf8(group), QString::fromUtf8(num2),
		              QString::fromUtf8(txt2), QString::fromUtf8(email),
		              QString::fromUtf8(sip), QString::fromUtf8(tel));
		if (ret == AT_OK)
			at_intermediate(m, "\r\n+CPBW: %u", *idx);
	}
	else
		ret = pb->edit(*idx, QString::fromUtf8(num), QString::fromUtf8(txt),
		               QString::fromUtf8(group), QString::fromUtf8(num2),
		               QString::fromUtf8(txt2), QString::fromUtf8(email),
		               QString::fromUtf8(sip), QString::fromUtf8(tel));
	return ret;
}


/*** Registration ***/
int QATPhonebook::registerPhonebook(at_commands_t *set, const char *id)
{
	return at_register_pb(set, id, NULL, readCb, writeCb, NULL, rangeCb, this);
}

void *at_plugin_register(at_commands_t *set)
{
	QATPhonebook *pb = new QATPhonebook;

	pb->registerPhonebook(set, "ME");
	return pb;
}

void at_plugin_unregister(void *data)
{
	QATPhonebook *pb = static_cast<QATPhonebook *>(data);
	delete pb;
}
