/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace Ui {
struct GroupCallBarContent;
} // namespace Ui

namespace Data {
class GroupCall;
class CloudImageView;
} // namespace Data

namespace style {
struct GroupCallUserpics;
} // namespace style

namespace HistoryView {

struct UserpicInRow {
	not_null<PeerData*> peer;
	bool speaking = false;
	mutable std::shared_ptr<Data::CloudImageView> view;
	mutable InMemoryKey uniqueKey;
};

void GenerateUserpicsInRow(
	QImage &result,
	const std::vector<UserpicInRow> &list,
	const style::GroupCallUserpics &st,
	int maxElements = 0);

class GroupCallTracker final {
public:
	explicit GroupCallTracker(not_null<PeerData*> peer);

	[[nodiscard]] rpl::producer<Ui::GroupCallBarContent> content() const;
	[[nodiscard]] rpl::producer<> joinClicks() const;

	[[nodiscard]] static rpl::producer<Ui::GroupCallBarContent> ContentByCall(
		not_null<Data::GroupCall*> call,
		int userpicSize);

private:
	const not_null<PeerData*> _peer;

	rpl::event_stream<> _joinClicks;

};

} // namespace HistoryView
