/**
 * Copyright (c) 2011-2026 libbitcoin developers
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "../../test.hpp"
#include "../../mocks/blocks.hpp"
#include "../../mocks/chunk_store.hpp"

BOOST_FIXTURE_TEST_SUITE(query_address_tests, test::directory_setup_fixture)

// get_confirmed_unspent_outpoints

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__turbo_genesis__empty)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, test::genesis_address0, true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__turbo_block1a_address0__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    // An unconfirmed output is listed only if its tx is pooled.
    const auto& spend2b = *test::block2b.transactions_ptr()->at(0);
    BOOST_REQUIRE(query.set_pooled(query.to_tx(spend2b.hash(false)), spend2b, test::context, 0));

    // tx4/5 contain one address output each, but are unpooled (and unconfirmable, block2a spends their prevouts).
    // block1b (unconfirmed) contains 2 address outputs, both spent by block2b (0 unspent).
    // block2b (unconfirmed) contains 1 address output, so 1 total address output.
    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, test::block1a_address0, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).value(), 0xb2u);
    BOOST_REQUIRE_EQUAL(out.at(0).point().hash(), test::block2b.transactions_ptr()->at(0)->hash(false));
}

static const system::hash_digest& genesis_coinbase() NOEXCEPT
{
    static const auto hash =
        test::genesis.transactions_ptr()->front()->hash(false);
    return hash;
}

static const system::chain::script& payee() NOEXCEPT
{
    static const system::chain::script script
    {
        { { system::chain::opcode::depth } }
    };
    return script;
}

static const system::hash_digest& payee_key() NOEXCEPT
{
    static const auto key = payee().hash();
    return key;
}

static system::chain::transaction to_spend(const system::hash_digest& hash,
    uint32_t index, uint64_t value,
    const system::chain::script& to={}) NOEXCEPT
{
    using namespace system::chain;
    const input input{ point{ hash, index }, script{}, max_uint32 };
    return { 1u, inputs{ input }, outputs{ output{ value, to } }, 0u };
}

static system::chain::transaction to_spend(const system::chain::points& points,
    uint64_t value, const system::chain::script& to={}) NOEXCEPT
{
    using namespace system::chain;
    inputs ins{};
    for (const auto& point: points)
        ins.emplace_back(point, script{}, max_uint32);

    return { 1u, std::move(ins), outputs{ output{ value, to } }, 0u };
}

static bool archive_pool(test::query_accessor& query,
    const system::chain::transaction& tx) NOEXCEPT
{
    const auto stored = query.set(tx);
    const auto link = query.to_tx(tx.hash(false));
    return stored && query.set_pooled(link, tx, test::context, 0);
}

// Confirm a block of a coinbase and the txs at height above parent.
static system::hash_digest archive_confirm(test::query_accessor& query,
    const system::hash_digest& parent, uint32_t height,
    const system::chain::transactions& txs) NOEXCEPT
{
    using namespace system::chain;
    const input null{ point{ system::null_hash, point::null_index }, script{},
        max_uint32 };
    transactions block_txs{ transaction{ 1u, inputs{ null },
        outputs{ output{ 42u, script{} } }, height } };
    block_txs.insert(block_txs.end(), txs.cbegin(), txs.cend());
    const block block{ header{ 1u, parent, system::null_hash, 1u, 2u, height },
        block_txs };
    const auto stored = query.set(block, database::context{ 0, height, 0 }, {},
        false, true);
    const auto link = query.to_header(block.hash());
    const auto confirmed = stored && query.push_candidate(link) &&
        query.push_confirmed(link, true);
    return confirmed ? block.hash() : system::null_hash;
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__unpooled__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(query.set(tx));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(tx.hash(false))));
    BOOST_REQUIRE(query.is_confirmable_tx(query.to_tx(tx.hash(false))));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__conflicted__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(archive_pool(query, conflicted));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);

    // The pooled tx is unconfirmable once its prevout is confirmed spent.
    const auto replacement = to_spend(genesis_coinbase(), 0, 41);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__conflicted_chain__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto parent = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto child = to_spend(parent.hash(false), 0, 41, payee());
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(archive_pool(query, child));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).point().hash(), child.hash(false));

    // The child is unconfirmable by ancestry (its parent's prevout is spent).
    const auto replacement = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__pooled_spender__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto parent = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(archive_pool(query, parent));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).point().hash(), parent.hash(false));

    const auto child = to_spend(parent.hash(false), 0, 41);
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__unpooled_spender__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    // An archived spender that is not pooled is not an unconfirmed spend.
    const auto parent = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto child = to_spend(parent.hash(false), 0, 41);
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(query.set(child));
    BOOST_REQUIRE(query.is_spent(query.to_output(parent.hash(false), 0)));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).point().hash(), parent.hash(false));
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_unspent_outpoints__conflicted_spender__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto funder = to_spend(system::one_hash, 0, 10);
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { funder });
    BOOST_REQUIRE(block1 != system::null_hash);

    const auto live = to_spend(funder.hash(false), 0, 9, payee());
    const auto spender = to_spend({ { live.hash(false), 0 }, { genesis_coinbase(), 0 } }, 50);
    BOOST_REQUIRE(archive_pool(query, live));
    BOOST_REQUIRE(archive_pool(query, spender));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());

    // The spender is unconfirmable once its other prevout is confirmed spent.
    const auto replacement = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_confirm(query, block1, 2, { replacement }) != system::null_hash);
    BOOST_REQUIRE(query.is_confirmable_tx(query.to_tx(live.hash(false))));
    BOOST_REQUIRE(!query.get_unconfirmed_unspent_outpoints(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).point().hash(), live.hash(false));
}

BOOST_AUTO_TEST_CASE(query_address__get_confirmed_unspent_outpoints__turbo_genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_confirmed_unspent_outpoints(cancel, out, test::genesis_address0, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BOOST_AUTO_TEST_CASE(query_address__get_confirmed_unspent_outpoints__genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_confirmed_unspent_outpoints(cancel, out, test::genesis_address0));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

// get_minimum_unspent_outpoints

BOOST_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__turbo_above__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 5000000001, true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__above__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 5000000001));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__at__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 5000000000));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BOOST_AUTO_TEST_CASE(query_address__get_minimum_unspent_outpoints__below__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 0));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));

    BOOST_REQUIRE(!query.get_minimum_unspent_outpoints(cancel, out, test::genesis_address0, 4999999999));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

// get_address_outpoints1

BOOST_AUTO_TEST_CASE(query_address__get_address_outpoints1__turbo_genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_address_outpoints(cancel, out, test::genesis_address0, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BOOST_AUTO_TEST_CASE(query_address__get_address_outpoints1__genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_address_outpoints(cancel, out, test::genesis_address0));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE(*out.cbegin() == query.get_outpoint(query.to_output(0, 0)));
}

BOOST_AUTO_TEST_CASE(query_address__get_address_outpoints1__cancel__query_canceled_false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    outpoints out{};
    std::atomic_bool cancel{ true };
    BOOST_REQUIRE_EQUAL(query.get_address_outpoints(cancel, out, test::genesis_address0), error::query_canceled);
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_SUITE_END()
