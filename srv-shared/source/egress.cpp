/* Orchid - WebRTC P2P VPN Market (on Ethereum)
 * Copyright (C) 2017-2019  The Orchid Authors
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */


#include "egress.hpp"
#include "forge.hpp"

namespace orc {

void Egress::Land(const Buffer &data) {
    Beam beam(data);
    auto span(beam.span());
    auto &ip4(span.cast<openvpn::IPv4Header>());
    auto length(openvpn::IPv4Header::length(ip4.version_len));

    switch (ip4.protocol) {
        case openvpn::IPCommon::TCP: {
            auto &tcp(span.cast<openvpn::TCPHeader>(length));
            Three target(openvpn::IPCommon::TCP, boost::endian::big_to_native(ip4.daddr), boost::endian::big_to_native(tcp.dest));
            auto translation(Find(target));
            if (translation == translations_.end())
                return;
            const auto &replace(translation->second.socket_);
            ForgeIP4(span, &openvpn::IPv4Header::daddr, replace.Host().to_v4().to_uint());
            Forge(tcp, &openvpn::TCPHeader::dest, replace.Port());
            return translation->second.drain_->Land(beam);
        } break;

        case openvpn::IPCommon::UDP: {
            auto &udp(span.cast<openvpn::UDPHeader>(length));
            Three target(openvpn::IPCommon::UDP, boost::endian::big_to_native(ip4.daddr), boost::endian::big_to_native(udp.dest));
            auto translation(Find(target));
            if (translation == translations_.end())
                return;
            const auto &replace(translation->second.socket_);
            ForgeIP4(span, &openvpn::IPv4Header::daddr, replace.Host().to_v4().to_uint());
            Forge(udp, &openvpn::UDPHeader::dest, replace.Port());
            return translation->second.drain_->Land(beam);
        } break;
    }
}

task<void> Translator::Send(const Buffer &data) {
    Beam beam(data);
    auto span(beam.span());
    auto &ip4(span.cast<openvpn::IPv4Header>());
    auto length(openvpn::IPv4Header::length(ip4.version_len));

    switch (ip4.protocol) {
        case openvpn::IPCommon::TCP: {
            auto &tcp(span.cast<openvpn::TCPHeader>(length));
            Three source(openvpn::IPCommon::TCP, boost::endian::big_to_native(ip4.saddr), boost::endian::big_to_native(tcp.source));
            auto translation(translations_.find(source));
            if (translation == translations_.end())
                translation = Translate(std::move(source));
            const auto &replace(translation->second);
            ForgeIP4(span, &openvpn::IPv4Header::saddr, replace.Host().to_v4().to_uint());
            Forge(tcp, &openvpn::TCPHeader::source, replace.Port());
            co_return co_await egress_->Send(beam);
        } break;

        case openvpn::IPCommon::UDP: {
            auto &udp(span.cast<openvpn::UDPHeader>(length));
            Three source(openvpn::IPCommon::UDP, boost::endian::big_to_native(ip4.saddr), boost::endian::big_to_native(udp.source));
            auto translation(translations_.find(source));
            if (translation == translations_.end())
                translation = Translate(std::move(source));
            const auto &replace(translation->second);
            ForgeIP4(span, &openvpn::IPv4Header::saddr, replace.Host().to_v4().to_uint());
            Forge(udp, &openvpn::UDPHeader::source, replace.Port());
            co_return co_await egress_->Send(beam);
        } break;
    }
}

}
