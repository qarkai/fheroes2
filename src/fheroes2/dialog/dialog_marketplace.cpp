/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2023                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2009 by Andrey Afletdinov <fheroes2@gmail.com>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "agg_image.h"
#include "cursor.h"
#include "dialog.h"
#include "game_hotkeys.h"
#include "icn.h"
#include "image.h"
#include "kingdom.h"
#include "localevent.h"
#include "math_base.h"
#include "players.h"
#include "resource.h"
#include "resource_trading.h"
#include "screen.h"
#include "settings.h"
#include "text.h"
#include "tools.h"
#include "translations.h"
#include "ui_button.h"
#include "ui_kingdom.h"
#include "ui_scrollbar.h"
#include "ui_tool.h"
#include "world.h"

namespace
{
    std::vector<fheroes2::Rect> GetResourceRects( const fheroes2::Point & pt )
    {
        std::vector<fheroes2::Rect> rects;
        rects.reserve( 7 );
        rects.emplace_back( pt.x, pt.y, 34, 34 ); // wood
        rects.emplace_back( pt.x + 37, pt.y, 34, 34 ); // mercury
        rects.emplace_back( pt.x + 74, pt.y, 34, 34 ); // ore
        rects.emplace_back( pt.x, pt.y + 37, 34, 34 ); // sulfur
        rects.emplace_back( pt.x + 37, pt.y + 37, 34, 34 ); // crystal
        rects.emplace_back( pt.x + 74, pt.y + 37, 34, 34 ); // gems
        rects.emplace_back( pt.x + 37, pt.y + 74, 34, 34 ); // gold
        return rects;
    }

    void AddResourceHeader( const fheroes2::Point & pt, const std::string & header )
    {
        Text text( header, Font::SMALL );
        text.Blit( pt.x + ( 108 - text.w() ) / 2, pt.y - 15 );
    }

    void RedrawResourceSprite( const fheroes2::Image & sf, int32_t px, int32_t py, const std::string & value )
    {
        fheroes2::Point dst_pt( px, py );

        fheroes2::Blit( sf, fheroes2::Display::instance(), dst_pt.x, dst_pt.y );

        if ( !value.empty() ) {
            Text text( value, Font::SMALL );
            dst_pt.x += ( 34 - text.w() ) / 2;
            dst_pt.y += 21;
            text.Blit( dst_pt.x, dst_pt.y );
        }
    }

    void RedrawResource( const fheroes2::Point & pt, const std::function<std::string( int )> & getValue )
    {
        const int tradpost = Settings::Get().isEvilInterfaceEnabled() ? ICN::TRADPOSE : ICN::TRADPOST;

        // wood
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 7 ), pt.x, pt.y, getValue( Resource::WOOD ) );
        // mercury
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 8 ), pt.x + 37, pt.y, getValue( Resource::MERCURY ) );
        // ore
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 9 ), pt.x + 74, pt.y, getValue( Resource::ORE ) );
        // sulfur
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 10 ), pt.x, pt.y + 37, getValue( Resource::SULFUR ) );
        // crystal
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 11 ), pt.x + 37, pt.y + 37, getValue( Resource::CRYSTAL ) );
        // gems
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 12 ), pt.x + 74, pt.y + 37, getValue( Resource::GEMS ) );
        // gold
        RedrawResourceSprite( fheroes2::AGG::GetICN( tradpost, 13 ), pt.x + 37, pt.y + 74, getValue( Resource::GOLD ) );
    }
}

void RedrawFromResource( const fheroes2::Point &, const Funds & );
void RedrawToResource( const fheroes2::Point & pt );
void RedrawToResource( const fheroes2::Point & pt, const uint32_t markets, int rs_from );
std::string GetStringTradeCosts( const uint32_t markets, int rs_from, int rs_to );

class TradeWindowGUI
{
public:
    explicit TradeWindowGUI( const fheroes2::Rect & rt )
        : pos_rt( rt )
        , back( fheroes2::Display::instance() )
        , tradpostIcnId( Settings::Get().isEvilInterfaceEnabled() ? ICN::TRADPOSE : ICN::TRADPOST )
        , _singlePlayer( false )
    {
        Settings & conf = Settings::Get();

        back.update( rt.x - 5, rt.y + 15, rt.width + 10, 160 );

        const bool isEvilInterface = conf.isEvilInterfaceEnabled();

        const int tradeButtonIcnID = isEvilInterface ? ICN::BUTTON_SMALL_TRADE_EVIL : ICN::BUTTON_SMALL_TRADE_GOOD;
        const int giftButtonIcnID = isEvilInterface ? ICN::BTNGIFT_EVIL : ICN::BTNGIFT_GOOD;

        buttonGift.setICNInfo( giftButtonIcnID, 0, 1 );
        buttonTrade.setICNInfo( tradeButtonIcnID, 0, 1 );
        buttonLeft.setICNInfo( tradpostIcnId, 3, 4 );
        buttonRight.setICNInfo( tradpostIcnId, 5, 6 );

        const fheroes2::Sprite & spriteGift = fheroes2::AGG::GetICN( giftButtonIcnID, 0 );
        const fheroes2::Sprite & spriteTrade = fheroes2::AGG::GetICN( tradeButtonIcnID, 0 );

        buttonGift.setPosition( pos_rt.x - 68 + ( pos_rt.width - spriteGift.width() ) / 2, pos_rt.y + pos_rt.height - spriteGift.height() );
        buttonTrade.setPosition( pos_rt.x + ( pos_rt.width - spriteTrade.width() ) / 2, pos_rt.y + 150 );
        buttonLeft.setPosition( pos_rt.x + 11, pos_rt.y + 129 );
        buttonRight.setPosition( pos_rt.x + 220, pos_rt.y + 129 );
        _scrollbar.setImage( fheroes2::AGG::GetICN( tradpostIcnId, 2 ) );
        _scrollbar.setArea( { pos_rt.x + ( pos_rt.width - fheroes2::AGG::GetICN( tradpostIcnId, 1 ).width() ) / 2 + 22, pos_rt.y + 131, 187, 11 } );
        _scrollbar.hide();

        const TextBox text( _( "Please inspect our fine wares. If you feel like offering a trade, click on the items you wish to trade with and for." ), Font::BIG,
                            fheroes2::Rect( pos_rt.x, pos_rt.y + 30, pos_rt.width, 100 ) );

        textSell.SetFont( Font::SMALL );
        textBuy.SetFont( Font::SMALL );

        const Players & players = conf.GetPlayers();
        int playerCount = 0;
        for ( const Player * player : players ) {
            if ( player != nullptr ) {
                const Kingdom & kingdom = world.GetKingdom( player->GetColor() );
                if ( kingdom.isPlay() )
                    ++playerCount;
            }
        }

        _singlePlayer = playerCount == 1;
    }

    void RedrawInfoBuySell( uint32_t count_sell, uint32_t count_buy, uint32_t max_sell, uint32_t orig_buy );
    void ShowTradeArea( const Kingdom & kingdom, const uint32_t markets, int resourceFrom, int resourceTo, uint32_t max_buy, uint32_t max_sell,
                        const bool firstExchange );

    fheroes2::Rect buttonMax;
    fheroes2::Rect buttonMin;
    fheroes2::Button buttonTrade;
    fheroes2::Button buttonLeft;
    fheroes2::Button buttonRight;
    fheroes2::Button buttonGift;
    fheroes2::Scrollbar _scrollbar;

private:
    fheroes2::Rect pos_rt;
    fheroes2::ImageRestorer back;
    const int tradpostIcnId;

    TextSprite textSell;
    TextSprite textBuy;
    bool _singlePlayer;
};

void TradeWindowGUI::ShowTradeArea( const Kingdom & kingdom, const uint32_t markets, int resourceFrom, int resourceTo, uint32_t max_buy, uint32_t max_sell,
                                    const bool firstExchange )
{
    fheroes2::Display & display = fheroes2::Display::instance();
    bool disable = kingdom.GetFunds().Get( resourceFrom ) <= 0;

    if ( disable || resourceFrom == resourceTo || ( Resource::GOLD != resourceTo && 0 == max_buy ) ) {
        _scrollbar.hide();
        back.restore();
        fheroes2::Rect dst_rt( pos_rt.x, pos_rt.y + 30, pos_rt.width, 100 );
        const std::string message = firstExchange && ( resourceFrom == resourceTo || 0 == max_buy )
                                        ? _( "Please inspect our fine wares. If you feel like offering a trade, click on the items you wish to trade with and for." )
                                        : _( "You have received quite a bargain. I expect to make no profit on the deal. Can I interest you in any of my other wares?" );

        const TextBox displayMessage( message, Font::BIG, dst_rt );

        if ( !_singlePlayer ) {
            buttonGift.enable();
        }
        buttonTrade.disable();
        buttonLeft.disable();
        buttonRight.disable();
        buttonGift.draw();
        buttonMax = fheroes2::Rect();
        buttonMin = fheroes2::Rect();
    }
    else {
        back.restore();

        const fheroes2::Sprite & bar = fheroes2::AGG::GetICN( tradpostIcnId, 1 );
        fheroes2::Point dst_pt( pos_rt.x + ( pos_rt.width - bar.width() ) / 2 - 2, pos_rt.y + 128 );
        fheroes2::Blit( bar, display, dst_pt.x, dst_pt.y );

        const uint32_t maximumValue = ( Resource::GOLD == resourceTo ) ? max_sell : max_buy;

        const fheroes2::Sprite & originalSlider = fheroes2::AGG::GetICN( tradpostIcnId, 2 );
        const fheroes2::Image scrollbarSlider = fheroes2::generateScrollbarSlider( originalSlider, true, 187, 1, static_cast<int32_t>( maximumValue + 1 ),
                                                                                   { 0, 0, 2, originalSlider.height() }, { 2, 0, 8, originalSlider.height() } );
        _scrollbar.setImage( scrollbarSlider );

        _scrollbar.setRange( 0, maximumValue );

        const uint32_t exchange_rate = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
        std::string message;
        if ( Resource::GOLD == resourceTo ) {
            message = _( "I can offer you %{count} for 1 unit of %{resfrom}." );
            StringReplace( message, "%{count}", exchange_rate );
            StringReplace( message, "%{resfrom}", Resource::String( resourceFrom ) );
        }
        else {
            message = _( "I can offer you 1 unit of %{resto} for %{count} units of %{resfrom}." );
            StringReplace( message, "%{resto}", Resource::String( resourceTo ) );
            StringReplace( message, "%{resfrom}", Resource::String( resourceFrom ) );
            StringReplace( message, "%{count}", exchange_rate );
        }
        const TextBox displayMessage( message, Font::BIG, { pos_rt.x, pos_rt.y + 30, pos_rt.width, 100 } );
        const fheroes2::Sprite & sprite_from = fheroes2::AGG::GetICN( ICN::RESOURCE, Resource::getIconIcnIndex( resourceFrom ) );
        dst_pt.x = pos_rt.x + ( pos_rt.width - sprite_from.width() + 1 ) / 2 - 70;
        dst_pt.y = pos_rt.y + 115 - sprite_from.height();
        fheroes2::Blit( sprite_from, display, dst_pt.x, dst_pt.y );
        const fheroes2::Sprite & sprite_to = fheroes2::AGG::GetICN( ICN::RESOURCE, Resource::getIconIcnIndex( resourceTo ) );
        dst_pt.x = pos_rt.x + ( pos_rt.width - sprite_to.width() + 1 ) / 2 + 70;
        dst_pt.y = pos_rt.y + 115 - sprite_to.height();
        fheroes2::Blit( sprite_to, display, dst_pt.x, dst_pt.y );
        const fheroes2::Sprite & sprite_fromto = fheroes2::AGG::GetICN( tradpostIcnId, 0 );
        dst_pt.x = pos_rt.x + ( pos_rt.width - sprite_fromto.width() ) / 2;
        dst_pt.y = pos_rt.y + 90;
        fheroes2::Blit( sprite_fromto, display, dst_pt.x, dst_pt.y );
        Text text( _( "Max" ), Font::YELLOW_SMALL );
        dst_pt.x = pos_rt.x + ( pos_rt.width - text.w() ) / 2 - 5;
        dst_pt.y = pos_rt.y + 80;
        buttonMax = fheroes2::Rect( dst_pt.x, dst_pt.y, text.w(), text.h() );
        text.Blit( dst_pt.x, dst_pt.y );
        text.Set( _( "Min" ), Font::YELLOW_SMALL );
        dst_pt.x = pos_rt.x + ( pos_rt.width - text.w() ) / 2 - 5;
        dst_pt.y = pos_rt.y + 103;
        buttonMin = fheroes2::Rect( dst_pt.x, dst_pt.y, text.w(), text.h() );
        text.Blit( dst_pt.x, dst_pt.y );
        text.Set( _( "Qty to trade" ), Font::SMALL );
        dst_pt.x = pos_rt.x + ( pos_rt.width - text.w() ) / 2;
        dst_pt.y = pos_rt.y + 115;
        text.Blit( dst_pt.x, dst_pt.y );

        buttonGift.enable();
        buttonTrade.enable();
        buttonLeft.enable();
        buttonRight.enable();

        buttonTrade.draw();
        buttonLeft.draw();
        buttonRight.draw();

        RedrawInfoBuySell( 0, 0, max_sell, kingdom.GetFunds().Get( resourceTo ) );
        _scrollbar.show();
    }

    display.render();
}

void TradeWindowGUI::RedrawInfoBuySell( uint32_t count_sell, uint32_t count_buy, uint32_t max_sell, uint32_t orig_buy )
{
    fheroes2::Point dst_pt;

    _scrollbar.hide();

    textSell.Hide();
    textSell.SetText( std::string( "-" ) + std::to_string( count_sell ) + " " + "(" + std::to_string( max_sell - count_sell ) + ")" );
    dst_pt.x = pos_rt.x + pos_rt.width / 2 - 70 - ( textSell.w() + 1 ) / 2;
    dst_pt.y = pos_rt.y + 116;
    textSell.SetPos( dst_pt.x, dst_pt.y );
    textSell.Show();

    textBuy.Hide();
    textBuy.SetText( std::string( "+" ) + std::to_string( count_buy ) + " " + "(" + std::to_string( orig_buy + count_buy ) + ")" );
    dst_pt.x = pos_rt.x + pos_rt.width / 2 + 70 - ( textBuy.w() + 1 ) / 2;
    dst_pt.y = pos_rt.y + 116;
    textBuy.SetPos( dst_pt.x, dst_pt.y );
    textBuy.Show();

    _scrollbar.show();
}

void Dialog::Marketplace( Kingdom & kingdom, bool fromTradingPost )
{
    fheroes2::Display & display = fheroes2::Display::instance();

    const bool isEvilInterface = Settings::Get().isEvilInterfaceEnabled();
    const int tradpost = isEvilInterface ? ICN::TRADPOSE : ICN::TRADPOST;
    const std::string & header = fromTradingPost ? _( "Trading Post" ) : _( "Marketplace" );
    const uint32_t markets = fromTradingPost ? 3 : kingdom.GetCountMarketplace();

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    Dialog::FrameBox box( 297, true );

    const fheroes2::Rect & pos_rt = box.GetArea();
    fheroes2::Point dst_pt( pos_rt.x, pos_rt.y );

    // header
    Text text;
    text.Set( header, Font::YELLOW_BIG );
    dst_pt.x = pos_rt.x + ( pos_rt.width - text.w() ) / 2;
    dst_pt.y = pos_rt.y;
    text.Blit( dst_pt.x, dst_pt.y );

    TradeWindowGUI gui( pos_rt );

    Funds fundsFrom = kingdom.GetFunds();
    int resourceFrom = 0;
    const fheroes2::Point pt1( pos_rt.x, pos_rt.y + 190 );
    std::vector<fheroes2::Rect> rectsFrom = GetResourceRects( pt1 );

    fheroes2::MovableSprite cursorFrom( fheroes2::AGG::GetICN( tradpost, 14 ) );
    AddResourceHeader( pt1, _( "Your Resources" ) );
    RedrawFromResource( pt1, fundsFrom );

    Funds fundsTo;
    int resourceTo = 0;
    const fheroes2::Point pt2( 136 + pos_rt.x, pos_rt.y + 190 );
    std::vector<fheroes2::Rect> rectsTo = GetResourceRects( pt2 );

    fheroes2::MovableSprite cursorTo( fheroes2::AGG::GetICN( tradpost, 14 ) );
    AddResourceHeader( pt2, _( "Available Trades" ) );
    RedrawToResource( pt2 );

    uint32_t count_sell = 0;
    uint32_t count_buy = 0;

    uint32_t max_sell = 0;
    uint32_t max_buy = 0;

    const fheroes2::Rect & buttonMax = gui.buttonMax;
    const fheroes2::Rect & buttonMin = gui.buttonMin;
    fheroes2::Button & buttonGift = gui.buttonGift;
    fheroes2::Button & buttonTrade = gui.buttonTrade;
    fheroes2::Button & buttonLeft = gui.buttonLeft;
    fheroes2::Button & buttonRight = gui.buttonRight;

    fheroes2::TimedEventValidator timedButtonLeft( [&buttonLeft]() { return buttonLeft.isPressed(); } );
    fheroes2::TimedEventValidator timedButtonRight( [&buttonRight]() { return buttonRight.isPressed(); } );
    buttonLeft.subscribe( &timedButtonLeft );
    buttonRight.subscribe( &timedButtonRight );

    fheroes2::Scrollbar & scrollbar = gui._scrollbar;

    // button exit
    const int exitButtonIcnID = isEvilInterface ? ICN::UNIFORM_EVIL_EXIT_BUTTON : ICN::UNIFORM_GOOD_EXIT_BUTTON;
    const fheroes2::Sprite & spriteExit = fheroes2::AGG::GetICN( exitButtonIcnID, 0 );

    dst_pt.x = pos_rt.x + 68 + ( pos_rt.width - spriteExit.width() ) / 2;
    dst_pt.y = pos_rt.y + pos_rt.height - spriteExit.height();
    fheroes2::Button buttonExit( dst_pt.x, dst_pt.y, exitButtonIcnID, 0, 1 );

    buttonGift.draw();
    buttonExit.draw();
    buttonTrade.disable();
    display.render();

    LocalEvent & le = LocalEvent::Get();

    bool firstExchange = true;

    // message loop
    while ( le.HandleEvents() ) {
        if ( buttonGift.isEnabled() )
            le.MousePressLeft( buttonGift.area() ) ? buttonGift.drawOnPress() : buttonGift.drawOnRelease();
        if ( buttonTrade.isEnabled() )
            le.MousePressLeft( buttonTrade.area() ) ? buttonTrade.drawOnPress() : buttonTrade.drawOnRelease();
        if ( buttonLeft.isEnabled() )
            le.MousePressLeft( buttonLeft.area() ) ? buttonLeft.drawOnPress() : buttonLeft.drawOnRelease();
        if ( buttonRight.isEnabled() )
            le.MousePressLeft( buttonRight.area() ) ? buttonRight.drawOnPress() : buttonRight.drawOnRelease();

        le.MousePressLeft( buttonExit.area() ) ? buttonExit.drawOnPress() : buttonExit.drawOnRelease();

        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() )
            break;

        // gift resources
        if ( buttonGift.isEnabled() && le.MouseClickLeft( buttonGift.area() ) ) {
            Dialog::MakeGiftResource( kingdom );

            resourceTo = Resource::UNKNOWN;
            resourceFrom = Resource::UNKNOWN;
            gui.ShowTradeArea( kingdom, markets, resourceFrom, resourceTo, 0, 0, firstExchange );

            cursorTo.hide();
            cursorFrom.hide();

            fundsFrom = kingdom.GetFunds();
            RedrawFromResource( pt1, fundsFrom );

            display.render();
        }

        // click from
        for ( uint32_t ii = 0; ii < rectsFrom.size(); ++ii ) {
            const fheroes2::Rect & rect_from = rectsFrom[ii];

            if ( le.MouseClickLeft( rect_from ) ) {
                resourceFrom = Resource::getResourceTypeFromIconIndex( ii );
                max_sell = fundsFrom.Get( resourceFrom );

                const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
                if ( tradeCost != 0 ) {
                    max_buy = ( Resource::GOLD == resourceTo ) ? ( max_sell * tradeCost ) : ( max_sell / tradeCost );
                }

                count_sell = 0;
                count_buy = 0;

                cursorFrom.show();
                cursorFrom.setPosition( rect_from.x - 2, rect_from.y - 2 );

                if ( resourceTo ) {
                    cursorTo.hide();
                }
                RedrawToResource( pt2, markets, resourceFrom );
                if ( resourceTo ) {
                    cursorTo.show();
                    gui.ShowTradeArea( kingdom, markets, resourceFrom, resourceTo, max_buy, max_sell, firstExchange );
                }

                display.render();
            }
            else if ( le.MousePressRight( rect_from ) ) {
                fheroes2::showKingdomIncome( kingdom, 0 );
            }
        }

        // click to
        for ( uint32_t ii = 0; ii < rectsTo.size(); ++ii ) {
            const fheroes2::Rect & rect_to = rectsTo[ii];

            if ( le.MouseClickLeft( rect_to ) ) {
                resourceTo = Resource::getResourceTypeFromIconIndex( ii );

                const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
                if ( tradeCost != 0 ) {
                    max_buy = ( Resource::GOLD == resourceTo ) ? ( max_sell * tradeCost ) : ( max_sell / tradeCost );
                }

                count_sell = 0;
                count_buy = 0;

                cursorTo.show();
                cursorTo.setPosition( rect_to.x - 2, rect_to.y - 2 );

                if ( resourceFrom ) {
                    cursorTo.hide();
                    RedrawToResource( pt2, markets, resourceFrom );
                    cursorTo.show();
                    gui.ShowTradeArea( kingdom, markets, resourceFrom, resourceTo, max_buy, max_sell, firstExchange );
                }
                display.render();
            }
        }

        // Scrollbar
        if ( buttonLeft.isEnabled() && buttonRight.isEnabled() && max_buy && le.MousePressLeft( scrollbar.getArea() ) ) {
            const fheroes2::Point & mousePos = le.GetMouseCursor();
            scrollbar.moveToPos( mousePos );
            const int32_t seek = scrollbar.currentIndex();

            const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
            count_buy = seek * ( Resource::GOLD == resourceTo ? tradeCost : 1 );
            count_sell = seek * ( Resource::GOLD == resourceTo ? 1 : tradeCost );

            gui.RedrawInfoBuySell( count_sell, count_buy, max_sell, fundsFrom.Get( resourceTo ) );
            display.render();
        }
        else if ( scrollbar.updatePosition() ) {
            display.render();
        }

        // click max
        if ( buttonMax.width && max_buy && le.MouseClickLeft( buttonMax ) ) {
            const int32_t max = scrollbar.maxIndex();

            const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
            count_buy = max * ( Resource::GOLD == resourceTo ? tradeCost : 1 );
            count_sell = max * ( Resource::GOLD == resourceTo ? 1 : tradeCost );

            scrollbar.moveToIndex( max );
            gui.RedrawInfoBuySell( count_sell, count_buy, max_sell, fundsFrom.Get( resourceTo ) );
            display.render();
        }

        // click min
        if ( buttonMin.width && max_buy && le.MouseClickLeft( buttonMin ) ) {
            const int32_t min = 1;

            const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
            count_buy = min * ( Resource::GOLD == resourceTo ? tradeCost : 1 );
            count_sell = min * ( Resource::GOLD == resourceTo ? 1 : tradeCost );

            scrollbar.moveToIndex( min );
            gui.RedrawInfoBuySell( count_sell, count_buy, max_sell, fundsFrom.Get( resourceTo ) );
            display.render();
        }

        // trade
        if ( buttonTrade.isEnabled() && le.MouseClickLeft( buttonTrade.area() ) && count_sell && count_buy ) {
            kingdom.OddFundsResource( Funds( resourceFrom, count_sell ) );
            kingdom.AddFundsResource( Funds( resourceTo, count_buy ) );

            firstExchange = false;

            resourceTo = Resource::UNKNOWN;
            resourceFrom = Resource::UNKNOWN;
            gui.ShowTradeArea( kingdom, markets, resourceFrom, resourceTo, 0, 0, firstExchange );

            fundsFrom = kingdom.GetFunds();
            cursorTo.hide();
            cursorFrom.hide();
            RedrawFromResource( pt1, fundsFrom );
            RedrawToResource( pt2 );
            display.render();
        }

        // decrease trade resource
        if ( count_buy
             && ( ( buttonLeft.isEnabled() && ( le.MouseClickLeft( gui.buttonLeft.area() ) || timedButtonLeft.isDelayPassed() ) )
                  || le.MouseWheelDn( scrollbar.getArea() ) ) ) {
            const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
            count_buy -= Resource::GOLD == resourceTo ? tradeCost : 1;
            count_sell -= Resource::GOLD == resourceTo ? 1 : tradeCost;

            scrollbar.backward();
            gui.RedrawInfoBuySell( count_sell, count_buy, max_sell, fundsFrom.Get( resourceTo ) );
            display.render();
        }

        // increase trade resource
        if ( count_buy < max_buy
             && ( ( buttonRight.isEnabled() && ( le.MouseClickLeft( buttonRight.area() ) || timedButtonRight.isDelayPassed() ) )
                  || le.MouseWheelUp( scrollbar.getArea() ) ) ) {
            const uint32_t tradeCost = fheroes2::getTradeCost( markets, resourceFrom, resourceTo );
            count_buy += Resource::GOLD == resourceTo ? tradeCost : 1;
            count_sell += Resource::GOLD == resourceTo ? 1 : tradeCost;

            scrollbar.forward();
            gui.RedrawInfoBuySell( count_sell, count_buy, max_sell, fundsFrom.Get( resourceTo ) );
            display.render();
        }
    }
}

void RedrawFromResource( const fheroes2::Point & pt, const Funds & rs )
{
    RedrawResource( pt, [&rs]( int rsType ) { return std::to_string( rs.Get( rsType ) ); } );
}

void RedrawToResource( const fheroes2::Point & pt )
{
    RedrawResource( pt, []( int ) { return ""; } );
}

void RedrawToResource( const fheroes2::Point & pt, const uint32_t markets, int rs_from )
{
    RedrawResource( pt, [markets, rs_from]( int rsType ) { return GetStringTradeCosts( markets, rs_from, rsType ); } );
}

std::string GetStringTradeCosts( const uint32_t markets, int rs_from, int rs_to )
{
    const uint32_t tradeCost = fheroes2::getTradeCost( markets, rs_from, rs_to );

    if ( tradeCost == 0 ) {
        return _( "n/a" );
    }

    std::string res = "1/";
    res.append( std::to_string( tradeCost ) );

    return res;
}
