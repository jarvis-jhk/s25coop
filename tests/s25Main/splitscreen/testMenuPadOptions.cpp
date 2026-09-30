// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

// BEFUND 4: dskOptions ist fuer das Pad freigeschaltet (dskOptions.h: WantsPadInput), war aber
// bisher der EINZIGE freigeschaltete Bildschirm ohne einen einzigen Nachweis - er liess sich in
// dieser Testumgebung nicht einmal konstruieren.
//
// DIE URSACHE, gefunden statt vermutet: dskOptions zeigt eine Sprachliste, und die kommt aus
// Languages::loadLanguages. Diese Stelle dereferenziert das Suchergebnis ihres Archivs
// UNGEPRUEFT (languages.cpp: dynamic_cast<...&>(*LOADER.GetArchive("languages").find(...))).
// Im Test gibt es das Archiv "languages" nicht - die Ressource liegt in den Spieldaten, die der
// Testlauf nicht aufloest -, also lief der Konstruktor in einen Nullzeigerzugriff.
//
// Behoben mit einem Ersatzarchiv in derselben Bauform wie LoadDummyGUIFiles und
// LoadDummyMapFiles: Loader::LoadDummyLanguageFiles. Damit ist der Bildschirm hier baubar und
// die Luecke geschlossen.
//
// Wie ueberall in diesen Nachweisen liegt die einzige Naht beim Treiber: der Test schreibt
// Padereignisse in MockupVideoDriver::padEvents_ und ruft WINDOWMANAGER.Draw().

#include "Loader.h"
#include "MenuPadFixture.h"
#include "RttrConfig.h"
#include "Settings.h"
#include "WindowManager.h"
#include "controls/ctrlButton.h"
#include "controls/ctrlComboBox.h"
#include "controls/ctrlEdit.h"
#include "controls/ctrlGroup.h"
#include "controls/ctrlOptionGroup.h"
#include "desktops/dskMainMenu.h"
#include "desktops/dskOptions.h"
#include "driver/KeyEvent.h"
#include "driver/MouseCoords.h"
#include "files.h"
#include "helpers/containerUtils.h"
#include "helpers/optional_io.h"
#include "ingameWindows/iwMsgbox.h"
#include "ingameWindows/iwMusicPlayer.h"
#include "input/PadRouter.h"
#include "rttr/test/ConfigOverride.hpp"
#include "rttr/test/TmpFolder.hpp"
#include <boost/nowide/fstream.hpp>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr PadButton Activate = PadButton::A;
constexpr PadButton NextCtrl = PadButton::RightShoulder;
constexpr PadDeviceId pad = 70;

struct OptionsPadFixture : rttr::test::MenuPadFixture
{
    /// dskOptions schreibt beim Verlassen SETTINGS.Save() - das darf NICHT in das echte
    /// Benutzerverzeichnis gehen. Dieselbe Absicherung wie in LocalGameFixture.
    rttr::test::TmpFolder userData_;
    rttr::test::ConfigOverride userDataOverride_{"USERDATA", userData_};

    OptionsPadFixture() { LOADER.LoadDummyLanguageFiles(); }

    void toOptions()
    {
        WINDOWMANAGER.Switch(std::make_unique<dskOptions>());
        frame();
        BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
    }

    /// Die Reiterleiste "Allgemein / Grafik / Sound". Gesucht wird sie ueber ihren TYP und nicht
    /// ueber eine Id - die Aufzaehlung in dskOptions.cpp ist privat, und ein Test, der ihre
    /// Zahlen abschreibt, prueft am Ende die Abschrift.
    static ctrlOptionGroup& tabs()
    {
        auto* dsk = desktopAs<dskOptions>();
        BOOST_TEST_REQUIRE(dsk != nullptr);
        const auto groups = dsk->GetCtrls<ctrlOptionGroup>();
        BOOST_TEST_REQUIRE(groups.size() == 1u);
        return *groups.front();
    }

    /// Faehrt den Fokus mit dem Schulterknopf weiter, bis er auf `target` steht.
    bool focusUntil(const Window* target, const unsigned maxSteps = 40)
    {
        for(unsigned i = 0; i < maxSteps; ++i)
        {
            if(focused(0) == target)
                return true;
            press(pad, NextCtrl);
        }
        return focused(0) == target; // LCOV_EXCL_LINE
    }
};

struct OptionsReturnFixture : OptionsPadFixture
{
    decltype(SETTINGS.lobby) savedLobby = SETTINGS.lobby;
    decltype(SETTINGS.server) savedServer = SETTINGS.server;
    ProxySettings savedProxy = SETTINGS.proxy;

    ~OptionsReturnFixture() override
    {
        WINDOWMANAGER.Switch(std::make_unique<Desktop>(nullptr));
        frame();
        SETTINGS.lobby = savedLobby;
        SETTINGS.server = savedServer;
        SETTINGS.proxy = savedProxy;
    }

    template<class T>
    static T& controlAt(const DrawPoint pos)
    {
        for(auto* group : desktopAs<dskOptions>()->GetCtrls<ctrlGroup>())
        {
            for(auto* ctrl : group->GetCtrls<T>())
            {
                if(ctrl->GetPos() == pos)
                    return *ctrl;
            }
        }
        throw std::runtime_error("Expected options control not found");
    }

    void typeInto(ctrlEdit& edit, const std::string& text)
    {
        // Route text through the same mouse focus and keyboard relay as a physical keyboard.
        WINDOWMANAGER.Msg_LeftDown(MouseCoords(edit.GetDrawRect().getOrigin() + DrawPoint(5, 5)));
        WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::End));
        const auto length = edit.GetText().size();
        for(size_t i = 0; i < length; ++i)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(KeyType::Backspace));
        for(const unsigned char c : text)
            WINDOWMANAGER.Msg_KeyDown(KeyEvent(c));
        BOOST_TEST_REQUIRE(edit.GetText() == text);
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(MenuPadOptionsTests)

/// Der Bildschirm laesst sich ueberhaupt bauen und nimmt ein Pad an. Genau das war bisher
/// unbewiesen - freigeschaltet, aber nie ausprobiert.
BOOST_FIXTURE_TEST_CASE(TheOptionsScreenAcceptsAPad, OptionsPadFixture)
{
    toOptions();
    pickUp(pad);
    BOOST_TEST(router().GetSlot(pad) == 0u);
    BOOST_TEST_REQUIRE(focused(0) != nullptr);

    // Und die Navigation bewegt sich wirklich - ein Bildschirm mit genau einer Fokusstation
    // waere mit dem Pad nicht bedienbar, obwohl er "bedienbar" hiesse.
    const Window* first = focused(0);
    press(pad, NextCtrl);
    BOOST_TEST(focused(0) != first);
}

/// Die drei Reiter sind mit dem Pad erreichbar UND umschaltbar. Ohne sie waere der halbe
/// Bildschirm (Grafik, Sound) vom Sofa aus unsichtbar.
BOOST_FIXTURE_TEST_CASE(TheOptionsTabsCanBeSwitchedWithThePad, OptionsPadFixture)
{
    toOptions();
    pickUp(pad);

    ctrlOptionGroup& group = tabs();
    const std::vector<ctrlButton*> tabButtons = group.GetCtrls<ctrlButton>();
    BOOST_TEST_REQUIRE(tabButtons.size() == 3u); // Allgemein, Grafik, Sound
    const unsigned before = group.GetSelection();

    // Ein Reiter, der NICHT der aktuelle ist.
    ctrlButton* target = nullptr;
    for(ctrlButton* bt : tabButtons)
    {
        if(bt->GetID() != before)
        {
            target = bt;
            break;
        }
    }
    BOOST_TEST_REQUIRE(target != nullptr);

    BOOST_TEST_REQUIRE(focusUntil(target), "the pad to reach another options tab");
    press(pad, Activate);
    frame();
    BOOST_TEST(group.GetSelection() == target->GetID());
    BOOST_TEST(group.GetSelection() != before);
}

/// Und der Weg zurueck ins Hauptmenue ist ebenfalls rein mit dem Pad begehbar - sonst waere der
/// Optionsbildschirm eine Sackgasse, aus der nur die Maus wieder herausfuehrt.
BOOST_FIXTURE_TEST_CASE(ThePadFindsTheWayBackFromTheOptions, OptionsPadFixture)
{
    toOptions();
    pickUp(pad);

    // "Zurueck" ist der erste fokussierbare Knopf des Bildschirms (dskOptions.cpp: ID_btBack ist
    // die erste freie Id nach dskMenuBase, und die Texte davor koennen keinen Fokus annehmen).
    BOOST_TEST_REQUIRE(focused(0) != nullptr);
    press(pad, Activate);
    for(int i = 0; i < 5; ++i)
        frame();
    BOOST_TEST(desktopAs<dskMainMenu>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(BUsesTheExistingSaveActionFromAnyFocusedControl, OptionsReturnFixture)
{
    toOptions();
    pickUp(pad);
    auto& name = controlAt<ctrlEdit>(DrawPoint(280, 70));
    typeInto(name, "CouchPlayer");
    BOOST_TEST_REQUIRE(focusUntil(&controlAt<ctrlComboBox>(DrawPoint(280, 100))));
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
    BOOST_TEST(SETTINGS.lobby.name == "CouchPlayer");
    // Read the saved file itself: a second Settings object would end the SETTINGS singleton when it is destroyed
    boost::nowide::ifstream persisted(RTTRCONFIG.ExpandPath(s25::resources::config));
    const std::string savedIni((std::istreambuf_iterator<char>(persisted)), std::istreambuf_iterator<char>());
    BOOST_TEST(savedIni.find("CouchPlayer") != std::string::npos);
    frame();
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST(video.padEvents_.empty());
}

BOOST_FIXTURE_TEST_CASE(BCancelsTheDropdownBeforeSavingAndLeaving, OptionsReturnFixture)
{
    toOptions();
    pickUp(pad);
    auto& portrait = controlAt<ctrlComboBox>(DrawPoint(280, 100));
    BOOST_TEST_REQUIRE(focusUntil(&portrait));
    const auto selected = portrait.GetSelection();
    const auto originalPortrait = SETTINGS.lobby.portraitIndex;
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(portrait.IsListOpen());
    press(pad, PadButton::DpadDown);
    BOOST_TEST_REQUIRE(portrait.GetSelection() != selected);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
    BOOST_TEST(!portrait.IsListOpen());
    BOOST_TEST(portrait.GetSelection() == selected);
    BOOST_TEST(SETTINGS.lobby.portraitIndex == originalPortrait);
    press(pad, PadButton::B);
    BOOST_TEST(desktopAs<dskMainMenu>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(BClosesTheMusicWindowBeforeLeavingTheOptions, OptionsReturnFixture)
{
    toOptions();
    pickUp(pad);
    const auto tabButtons = tabs().GetCtrls<ctrlButton>();
    BOOST_TEST_REQUIRE(focusUntil(tabButtons.back()));
    press(pad, PadButton::A);
    auto& musicButton = controlAt<ctrlButton>(DrawPoint(280, 220));
    BOOST_TEST_REQUIRE(focusUntil(&musicButton));
    press(pad, PadButton::A);
    BOOST_TEST_REQUIRE(dynamic_cast<iwMusicPlayer*>(WINDOWMANAGER.GetTopMostWindow()) != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
    BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
    press(pad, PadButton::B);
    BOOST_TEST(desktopAs<dskMainMenu>() != nullptr);
}

BOOST_FIXTURE_TEST_CASE(BRetainsPortValidationAndItsRequiredConfirmation, OptionsReturnFixture)
{
    for(const bool proxy : {false, true})
    {
        SETTINGS.server.localPort = proxy ? 12345 : 0;
        SETTINGS.proxy.port = proxy ? 0 : 1080;
        padInput().Reset();
        toOptions();
        pickUp(pad);
        press(pad, PadButton::B);
        auto* error = dynamic_cast<iwMsgbox*>(WINDOWMANAGER.GetTopMostWindow());
        BOOST_TEST_REQUIRE(error != nullptr);
        BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == error);
        press(pad, PadButton::A);
        BOOST_TEST_REQUIRE(WINDOWMANAGER.GetTopMostWindow() == nullptr);
        BOOST_TEST_REQUIRE(desktopAs<dskOptions>() != nullptr);
        auto& port = controlAt<ctrlEdit>(proxy ? DrawPoint(480, 270) : DrawPoint(280, 200));
        typeInto(port, proxy ? "1080" : "12345");
        press(pad, PadButton::B);
        BOOST_TEST_REQUIRE(desktopAs<dskMainMenu>() != nullptr);
    }
}

BOOST_FIXTURE_TEST_CASE(StartDoesNotSaveOrLeaveOptions, OptionsReturnFixture)
{
    toOptions();
    pickUp(pad);
    press(pad, PadButton::Start);
    BOOST_TEST(desktopAs<dskOptions>() != nullptr);
    BOOST_TEST(WINDOWMANAGER.GetTopMostWindow() == nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
