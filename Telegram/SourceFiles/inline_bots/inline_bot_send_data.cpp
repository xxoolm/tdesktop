/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "inline_bots/inline_bot_send_data.h"

#include "api/api_text_entities.h"
#include "data/data_document.h"
#include "inline_bots/inline_bot_result.h"
#include "storage/localstorage.h"
#include "lang/lang_keys.h"
#include "history/history.h"
#include "history/history_message.h"
#include "data/data_channel.h"
#include "app.h"

namespace InlineBots {
namespace internal {

QString SendData::getLayoutTitle(const Result *owner) const {
	return owner->_title;
}

QString SendData::getLayoutDescription(const Result *owner) const {
	return owner->_description;
}

void SendDataCommon::addToHistory(
		const Result *owner,
		not_null<History*> history,
		MTPDmessage::Flags flags,
		MTPDmessage_ClientFlags clientFlags,
		MsgId msgId,
		PeerId fromId,
		MTPint mtpDate,
		UserId viaBotId,
		MsgId replyToId,
		const QString &postAuthor,
		const MTPReplyMarkup &markup) const {
	auto fields = getSentMessageFields();
	if (!fields.entities.v.isEmpty()) {
		flags |= MTPDmessage::Flag::f_entities;
	}
	auto action = Api::SendAction(history);
	action.replyTo = replyToId;
	const auto replyHeader = NewMessageReplyHeader(action);
	if (replyToId) {
		flags |= MTPDmessage::Flag::f_reply_to;
	}
	const auto views = 1;
	const auto forwards = 0;
	history->addNewMessage(
		MTP_message(
			MTP_flags(flags),
			MTP_int(msgId),
			peerToMTP(fromId),
			peerToMTP(history->peer->id),
			MTPMessageFwdHeader(),
			MTP_int(viaBotId.bare), // #TODO ids
			replyHeader,
			mtpDate,
			fields.text,
			fields.media,
			markup,
			fields.entities,
			MTP_int(views),
			MTP_int(forwards),
			MTPMessageReplies(),
			MTPint(), // edit_date
			MTP_string(postAuthor),
			MTPlong(),
			//MTPMessageReactions(),
			MTPVector<MTPRestrictionReason>(),
			MTPint()), // ttl_period
		clientFlags,
		NewMessageType::Unread);
}

QString SendDataCommon::getErrorOnSend(
		const Result *owner,
		not_null<History*> history) const {
	const auto error = Data::RestrictionError(
		history->peer,
		ChatRestriction::SendMessages);
	return error.value_or(QString());
}

SendDataCommon::SentMTPMessageFields SendText::getSentMessageFields() const {
	SentMTPMessageFields result;
	result.text = MTP_string(_message);
	result.entities = Api::EntitiesToMTP(&session(), _entities);
	return result;
}

SendDataCommon::SentMTPMessageFields SendGeo::getSentMessageFields() const {
	SentMTPMessageFields result;
	if (_period) {
		using Flag = MTPDmessageMediaGeoLive::Flag;
		result.media = MTP_messageMediaGeoLive(
			MTP_flags((_heading ? Flag::f_heading : Flag(0))
				| (_proximityNotificationRadius ? Flag::f_proximity_notification_radius : Flag(0))),
			_location.toMTP(),
			MTP_int(_heading.value_or(0)),
			MTP_int(*_period),
			MTP_int(_proximityNotificationRadius.value_or(0)));
	} else {
		result.media = MTP_messageMediaGeo(_location.toMTP());
	}
	return result;
}

SendDataCommon::SentMTPMessageFields SendVenue::getSentMessageFields() const {
	SentMTPMessageFields result;
	auto venueType = QString();
	result.media = MTP_messageMediaVenue(
		_location.toMTP(),
		MTP_string(_title),
		MTP_string(_address),
		MTP_string(_provider),
		MTP_string(_venueId),
		MTP_string(venueType));
	return result;
}

SendDataCommon::SentMTPMessageFields SendContact::getSentMessageFields() const {
	SentMTPMessageFields result;
	const auto userId = 0;
	const auto vcard = QString();
	result.media = MTP_messageMediaContact(
		MTP_string(_phoneNumber),
		MTP_string(_firstName),
		MTP_string(_lastName),
		MTP_string(vcard),
		MTP_int(userId));
	return result;
}

QString SendContact::getLayoutDescription(const Result *owner) const {
	auto result = SendData::getLayoutDescription(owner);
	if (result.isEmpty()) {
		return App::formatPhone(_phoneNumber);
	}
	return result;
}

void SendPhoto::addToHistory(
		const Result *owner,
		not_null<History*> history,
		MTPDmessage::Flags flags,
		MTPDmessage_ClientFlags clientFlags,
		MsgId msgId,
		PeerId fromId,
		MTPint mtpDate,
		UserId viaBotId,
		MsgId replyToId,
		const QString &postAuthor,
		const MTPReplyMarkup &markup) const {
	history->addNewLocalMessage(
		msgId,
		flags,
		clientFlags,
		viaBotId,
		replyToId,
		mtpDate.v,
		fromId,
		postAuthor,
		_photo,
		{ _message, _entities },
		markup);
}

QString SendPhoto::getErrorOnSend(
		const Result *owner,
		not_null<History*> history) const {
	const auto error = Data::RestrictionError(
		history->peer,
		ChatRestriction::SendMedia);
	return error.value_or(QString());
}

void SendFile::addToHistory(
		const Result *owner,
		not_null<History*> history,
		MTPDmessage::Flags flags,
		MTPDmessage_ClientFlags clientFlags,
		MsgId msgId,
		PeerId fromId,
		MTPint mtpDate,
		UserId viaBotId,
		MsgId replyToId,
		const QString &postAuthor,
		const MTPReplyMarkup &markup) const {
	history->addNewLocalMessage(
		msgId,
		flags,
		clientFlags,
		viaBotId,
		replyToId,
		mtpDate.v,
		fromId,
		postAuthor,
		_document,
		{ _message, _entities },
		markup);
}

QString SendFile::getErrorOnSend(
		const Result *owner,
		not_null<History*> history) const {
	const auto errorMedia = Data::RestrictionError(
		history->peer,
		ChatRestriction::SendMedia);
	const auto errorStickers = Data::RestrictionError(
		history->peer,
		ChatRestriction::SendStickers);
	const auto errorGifs = Data::RestrictionError(
		history->peer,
		ChatRestriction::SendGifs);
	return errorMedia
		? *errorMedia
		: (errorStickers && (_document->sticker() != nullptr))
		? *errorStickers
		: (errorGifs
			&& _document->isAnimation()
			&& !_document->isVideoMessage())
		? *errorGifs
		: QString();
}

void SendGame::addToHistory(
		const Result *owner,
		not_null<History*> history,
		MTPDmessage::Flags flags,
		MTPDmessage_ClientFlags clientFlags,
		MsgId msgId,
		PeerId fromId,
		MTPint mtpDate,
		UserId viaBotId,
		MsgId replyToId,
		const QString &postAuthor,
		const MTPReplyMarkup &markup) const {
	history->addNewLocalMessage(
		msgId,
		flags,
		clientFlags,
		viaBotId,
		replyToId,
		mtpDate.v,
		fromId,
		postAuthor,
		_game,
		markup);
}

QString SendGame::getErrorOnSend(
		const Result *owner,
		not_null<History*> history) const {
	const auto error = Data::RestrictionError(
		history->peer,
		ChatRestriction::SendGames);
	return error.value_or(QString());
}

auto SendInvoice::getSentMessageFields() const -> SentMTPMessageFields {
	SentMTPMessageFields result;
	result.media = _media;
	return result;
}

QString SendInvoice::getLayoutDescription(const Result *owner) const {
	return qs(_media.c_messageMediaInvoice().vdescription());
}

} // namespace internal
} // namespace InlineBots
