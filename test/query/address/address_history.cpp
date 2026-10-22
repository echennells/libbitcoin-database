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

// get_history

BOOST_AUTO_TEST_CASE(query_address__get_history__genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    const std::atomic_bool cancel{};
    const auto& hash = test::genesis_address0;
    BOOST_CHECK(!query.get_unconfirmed_history(cancel, out, hash));
    BOOST_CHECK_EQUAL(out.size(), 0u);

    out.clear();
    height_link cursor{};
    BOOST_CHECK(!query.get_confirmed_history(cancel, cursor, out, hash));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);
    BOOST_CHECK_EQUAL(out.at(0).position, 0u);
    BOOST_CHECK_EQUAL(out.at(0).tx.height(), 0u);
    BOOST_CHECK_EQUAL(out.at(0).tx.hash(), test::genesis.transactions_ptr()->at(0)->hash(false));

    out.clear();
    cursor = {};
    BOOST_CHECK(!query.get_history(cancel, cursor, out, hash));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);
    BOOST_CHECK_EQUAL(out.at(0).position, 0u);
    BOOST_CHECK_EQUAL(out.at(0).tx.height(), 0u);
    BOOST_CHECK_EQUAL(out.at(0).tx.hash(), test::genesis.transactions_ptr()->at(0)->hash(false));
}

// Pool the archived tx, populated for its pooled fee.
static bool pool_archived(test::query_accessor& query,
    const system::chain::transaction& tx) NOEXCEPT
{
    const auto link = query.to_tx(tx.hash(false));
    return query.populate_with_metadata(tx) &&
        query.set_pooled(link, tx, test::context, 0);
}

// Pool the block2b tx, a copy since population would mutate the shared mock.
static bool pool_block2b(test::query_accessor& query) NOEXCEPT
{
    const auto& source = *test::block2b.transactions_ptr()->front();
    const system::chain::transaction tx{ source.to_data(true), true };
    return pool_archived(query, tx);
}

BOOST_AUTO_TEST_CASE(query_address__get_history__turbo_block1a_address0__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));
    BOOST_CHECK(pool_block2b(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_CHECK(!query.get_history(cancel, cursor, out, test::block1a_address0, max_size_t, true));

    // tx4/tx5 (unconfirmable) and tx7 (coinbase) are not pooled.
    BOOST_REQUIRE_EQUAL(out.size(), 5u);

    // Identities (not part of sort).
    BOOST_CHECK_EQUAL(out.at(0).tx.hash(), test::block1a.transactions_ptr()->at(0)->hash(false)); // tx1/tx1 (same)
    BOOST_CHECK_EQUAL(out.at(1).tx.hash(), test::block2a.transactions_ptr()->at(0)->hash(false)); // tx2/tx2 (same)
    BOOST_CHECK_EQUAL(out.at(2).tx.hash(), test::block2a.transactions_ptr()->at(1)->hash(false)); // tx3/tx3 (same)
    BOOST_CHECK_EQUAL(out.at(3).tx.hash(), test::block3a.transactions_ptr()->at(0)->hash(false)); // tx6/tx6 (same)
    BOOST_CHECK_EQUAL(out.at(4).tx.hash(), test::block2b.transactions_ptr()->at(0)->hash(false)); // tx8

    // Confirmed by height ascending.
    // Unconfirmed rooted before unrooted.
    BOOST_CHECK_EQUAL(out.at(0).tx.height(), 1u);                             // tx1
    BOOST_CHECK_EQUAL(out.at(1).tx.height(), 2u);                             // tx2
    BOOST_CHECK_EQUAL(out.at(2).tx.height(), 2u);                             // tx3
    BOOST_CHECK_EQUAL(out.at(3).tx.height(), 3u);                             // tx6
    BOOST_CHECK_EQUAL(out.at(4).tx.height(), history::unrooted_height);       // tx8

    // Confirmed height by block position.
    BOOST_CHECK_EQUAL(out.at(0).position, 0u);                                // tx1
    BOOST_CHECK_EQUAL(out.at(1).position, 0u);                                // tx2
    BOOST_CHECK_EQUAL(out.at(2).position, 1u);                                // tx3
    BOOST_CHECK_EQUAL(out.at(3).position, 0u);                                // tx6
    BOOST_CHECK_EQUAL(out.at(4).position, history::unconfirmed_position);     // tx8

    // Fee (not part of sort).
    using namespace system;
    BOOST_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);               // tx1
    BOOST_CHECK_EQUAL(out.at(1).fee, history::missing_prevout);               // tx2
    BOOST_CHECK_EQUAL(out.at(2).fee, history::missing_prevout);               // tx3
    BOOST_CHECK_EQUAL(out.at(3).fee, history::missing_prevout);               // tx6
    BOOST_CHECK_EQUAL(out.at(4).fee, floored_subtract(0xb1u + 0xb1u, 0xb2u)); // tx8
}

BOOST_AUTO_TEST_CASE(query_address__get_history__limited__depth_limited_empty)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));
    BOOST_CHECK(pool_block2b(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching the address cursor (or terminal).
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    cursor = {};
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    // tx4/tx5 (unconfirmable) and tx7 (coinbase) are not pooled.
    cursor = {};
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 9, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 5u);

    cursor = {};
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 100, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 5u);
}

BOOST_AUTO_TEST_CASE(query_address__get_history__start__excludes_prior)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));
    BOOST_CHECK(pool_block2b(query));

    histories out{};
    height_link cursor{ 42 };
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching the address cursor (or terminal).
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    cursor = 42;
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    // Only unconfirmed, tx4/tx5 (unconfirmable) and tx7 (coinbase) are not pooled.
    cursor = 42;
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 9, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 1u);
        
    // Only excludes genesis.
    cursor = 1;
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 100, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 5u);

    // Excludes block1a which has one confirmed touching tx.
    cursor = 2;
    BOOST_CHECK_EQUAL(query.get_history(cancel, cursor, out, hash, 100, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 4u);
}

static const system::hash_digest& genesis_coinbase() NOEXCEPT
{
    static const auto hash =
        test::genesis.transactions_ptr()->front()->hash(false);
    return hash;
}

static const system::chain::script& payee() NOEXCEPT
{
    using namespace system::chain;
    static const script output_script{ { { opcode::depth } } };
    return output_script;
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

static system::chain::transaction to_fan(const system::hash_digest& hash,
    uint32_t index, size_t count) NOEXCEPT
{
    using namespace system::chain;
    const input input{ point{ hash, index }, script{}, max_uint32 };
    return { 1u, inputs{ input }, outputs(count, output{ 1u, script{} }), 0u };
}

static bool archive_pool(test::query_accessor& query,
    const system::chain::transaction& tx) NOEXCEPT
{
    return query.set(tx) && pool_archived(query, tx);
}

// Confirm a block of a coinbase (paying to) and the txs above parent.
static system::hash_digest archive_confirm(test::query_accessor& query,
    const system::hash_digest& parent, uint32_t height,
    const system::chain::transactions& txs,
    const system::chain::script& to={}) NOEXCEPT
{
    using namespace system::chain;
    const input null{ point{ system::null_hash, point::null_index }, script{},
        max_uint32 };
    transactions block_txs{ transaction{ 1u, inputs{ null },
        outputs{ output{ 42u, to } }, height } };
    block_txs.insert(block_txs.end(), txs.cbegin(), txs.cend());
    const block block{ header{ 1u, parent, system::null_hash, 1u, 2u, height },
        block_txs };
    const auto stored = query.set(block, database::context{ 0, height, 0 },
        {}, false, true);
    const auto link = query.to_header(block.hash());
    const auto confirmed = stored && query.push_candidate(link) &&
        query.push_confirmed(link, true);
    return confirmed ? block.hash() : system::null_hash;
}

BOOST_AUTO_TEST_CASE(query_address__get_history__conflicted__replacement_only)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto replacement = to_spend(genesis_coinbase(), 0, 41, payee());
    BOOST_REQUIRE(archive_pool(query, conflicted));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), replacement.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_history__dead_chain__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto root = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto child = to_spend(root.hash(false), 0, 41, payee());
    const auto grandchild = to_spend(child.hash(false), 0, 40, payee());
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(archive_pool(query, grandchild));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 3u);

    // The descendants of a conflicted root are unconfirmable by ancestry.
    const auto conflict = to_spend(genesis_coinbase(), 0, 39);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);

    cursor = {};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_history__dead_ancestry__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Only the descendants pay the address, the dead ancestry is not listed.
    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto middle = to_spend(root.hash(false), 0, 41);
    const auto first = to_spend(middle.hash(false), 0, 40, payee());
    const auto second = to_spend(first.hash(false), 0, 39, payee());
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, middle));
    BOOST_REQUIRE(archive_pool(query, first));
    BOOST_REQUIRE(archive_pool(query, second));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 2u);

    const auto conflict = to_spend(genesis_coinbase(), 0, 38);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);

    cursor = {};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_history__dead_fanout__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Independent descendants of one dead ancestry, which is not listed.
    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto middle = to_spend(root.hash(false), 0, 41);
    const auto fan = to_fan(middle.hash(false), 0, 3);
    const auto first = to_spend(fan.hash(false), 0, 1, payee());
    const auto second = to_spend(fan.hash(false), 1, 1, payee());
    const auto third = to_spend(fan.hash(false), 2, 1, payee());
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, middle));
    BOOST_REQUIRE(archive_pool(query, fan));
    BOOST_REQUIRE(archive_pool(query, first));
    BOOST_REQUIRE(archive_pool(query, second));
    BOOST_REQUIRE(archive_pool(query, third));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 3u);

    const auto conflict = to_spend(genesis_coinbase(), 0, 38);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);

    cursor = {};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_history__popped_coinbase__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, {}, payee());
    BOOST_REQUIRE(block1 != system::null_hash);

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), 1u);

    const auto coinbase = query.to_coinbase(query.to_header(block1));
    BOOST_REQUIRE(query.pop_confirmed());

    // A popped coinbase is confirmable but is never pooled.
    BOOST_REQUIRE(!query.is_pooled(coinbase));
    BOOST_REQUIRE(query.is_confirmable_tx(coinbase));

    cursor = {};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_history__turbo_mixed_pool__sequential_parity)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto funder0 = to_spend(system::one_hash, 0, 10);
    const auto funder1 = to_spend(system::one_hash, 1, 10);
    const auto funder2 = to_spend(system::one_hash, 2, 10);
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { funder0, funder1, funder2 });
    BOOST_REQUIRE(block1 != system::null_hash);

    const auto live = to_spend(funder0.hash(false), 0, 9, payee());
    const auto dead = to_spend(funder1.hash(false), 0, 9, payee());
    const auto child = to_spend(dead.hash(false), 0, 8, payee());
    const auto unpooled = to_spend(funder2.hash(false), 0, 9, payee());
    const auto replacement = to_spend(funder1.hash(false), 0, 7, payee());
    BOOST_REQUIRE(archive_pool(query, live));
    BOOST_REQUIRE(archive_pool(query, dead));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(query.set(unpooled));
    BOOST_REQUIRE(archive_confirm(query, block1, 2, { replacement }) != system::null_hash);

    histories sequential{};
    histories parallel{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, sequential, payee_key(), max_size_t, false));

    cursor = {};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, parallel, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(sequential.size(), 2u);
    BOOST_REQUIRE_EQUAL(sequential.at(0).tx.hash(), replacement.hash(false));
    BOOST_REQUIRE_EQUAL(sequential.at(1).tx.hash(), live.hash(false));
    BOOST_REQUIRE(sequential == parallel);
}

BOOST_AUTO_TEST_CASE(query_address__get_history__pooled_then_confirmed__confirmed_only)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Pooling latched so that the block reuses the pooled tx.
    store.set_pooling();
    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(archive_pool(query, tx));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { tx }) != system::null_hash);

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_history(cancel, cursor, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), tx.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 1u);

    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE(out.empty());
}

// ----------------------------------------------------------------------------

// block1a_address0 has 9 instances `script{ { { opcode::pick } } }`.
// 7 outputs are in individual txs, 2 are in tx bk1b0.
// 4 owning txs are confirmed (bk1a/bk2a/bk3a).
// 4 owning txs are unconfirmed (bk1b/bk2b/tx4/tx5).
// 6 outputs are spent, (bk2a/bk3a/tx4/tx5/bk2b00/bk2b01).
// 2 spend txs are confirmed, (bk2a/bk3a).
// 3 spend txs are unconfirmed, (tx4/tx5/bk2b0(2)).
//
// One owner owns two (one owner removed by deduplication).
// All spenders are also owners (all spends removed by deduplication).
// 4 owns + 2 spend txs are confirmed.
// 4 owns + 2 spend txs are confirmed.
//
// 4 unconfirmed:
// tx4     spends 1u + owns 1u (+1u) [unconfirmed rooted].
// tx5     spends 1u + owns 1u (+1u) [unconfirmed rooted].
// bk1btx0             owns 2u (+1u) [unconfirmed unrooted].
// bk2btx0 spends 2u + owns 1u (+1u) [unconfirmed unrooted].
//
// 4 confirmed:
// bk1atx0             owns 1c (+1c) [confirmed].
// bk2atx0 spends 1c + owns 1c (+1c) [confirmed].
// bk2atx1             owns 1c (+1c) [confirmed].
// bk3atx0 spends 1c + owns 1c (+1c) [confirmed].
//
// Unconfirmed history is pooled and confirmable txs. Tests pool bk2btx0 only,
// as tx4/tx5 are unconfirmable (bk2atx0/bk3atx0 spend the same outputs) and
// bk1btx0 is a coinbase.

// get_unconfirmed_history

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__turbo_block1a_address0__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));
    BOOST_CHECK(pool_block2b(query));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_CHECK(!query.get_unconfirmed_history(cancel, out, test::block1a_address0, max_size_t, true));

    // tx4/tx5 (unconfirmable) and tx7 (coinbase) are not pooled.
    BOOST_REQUIRE_EQUAL(out.size(), 1u);

    // Identities (not part of sort).
    BOOST_CHECK_EQUAL(out.at(0).tx.hash(), test::block2b.transactions_ptr()->at(0)->hash(false)); // tx8

    // Confirmed by height ascending (not part of sort).

    // Unconfirmed rooted before unrooted.
    BOOST_CHECK_EQUAL(out.at(0).tx.height(), history::unrooted_height); // block2b spends block1b

    // Confirmed height by block position (not part of sort).
    BOOST_CHECK_EQUAL(out.at(0).position, history::unconfirmed_position);

    // Fee (not part of sort).
    using namespace system;
    BOOST_CHECK_EQUAL(out.at(0).fee, floored_subtract(0xb1u + 0xb1u, 0xb2u));
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__limited__depth_limited_empty)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));
    BOOST_CHECK(pool_block2b(query));

    histories out{};
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching the address cursor (or terminal).
    BOOST_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 0, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    BOOST_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 8, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    // tx4/tx5 (unconfirmable) and tx7 (coinbase) are not pooled.
    BOOST_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 9, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 1u);

    BOOST_CHECK_EQUAL(query.get_unconfirmed_history(cancel, out, hash, 100, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__rooted_and_unrooted__sorted)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto rooted = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto unrooted = to_spend(rooted.hash(false), 0, 41, payee());
    BOOST_REQUIRE(archive_pool(query, rooted));
    BOOST_REQUIRE(archive_pool(query, unrooted));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), rooted.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), history::rooted_height);
    BOOST_REQUIRE_EQUAL(out.at(0).fee, 5'000'000'000u - 42u);
    BOOST_REQUIRE_EQUAL(out.at(1).tx.hash(), unrooted.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(1).tx.height(), history::unrooted_height);
    BOOST_REQUIRE_EQUAL(out.at(1).fee, 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__pooled__rooted)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto reward = test::genesis.transactions_ptr()->front()->spend();
    BOOST_REQUIRE(archive_pool(query, tx));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), tx.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), history::rooted_height);
    BOOST_REQUIRE_EQUAL(out.at(0).position, history::unconfirmed_position);
    BOOST_REQUIRE_EQUAL(out.at(0).fee, reward - 42u);
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__conflicted__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto replacement = to_spend(genesis_coinbase(), 0, 41, payee());
    BOOST_REQUIRE(archive_pool(query, conflicted));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);

    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__conflicted_parent__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    const auto child = to_spend(parent.hash(false), 0, 41, payee());
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(archive_pool(query, child));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), child.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), history::unrooted_height);

    const auto conflict = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__dead_chain__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto root = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto child = to_spend(root.hash(false), 0, 41, payee());
    const auto grandchild = to_spend(child.hash(false), 0, 40, payee());
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(archive_pool(query, grandchild));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE_EQUAL(out.size(), 3u);

    // The descendants of a conflicted root are unconfirmable by ancestry.
    const auto conflict = to_spend(genesis_coinbase(), 0, 39);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, false));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__unpooled__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(query.set(tx));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(tx.hash(false))));
    BOOST_REQUIRE(query.is_confirmable_tx(query.to_tx(tx.hash(false))));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__unpooled_parent__unrooted)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Confirmability of ancestors does not require that they are pooled.
    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    const auto child = to_spend(parent.hash(false), 0, 41, payee());
    BOOST_REQUIRE(query.set(parent));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(parent.hash(false))));
    BOOST_REQUIRE(query.is_confirmable_tx(query.to_tx(parent.hash(false))));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), child.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), history::unrooted_height);
    BOOST_REQUIRE_EQUAL(out.at(0).position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__pool_disabled__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    settings.pool.buckets = 0;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Without a pool table no tx is pooled (set_pooled is a no-op).
    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(archive_pool(query, tx));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(tx.hash(false))));

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unconfirmed_history__conflict_popped__restored)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto conflict = to_spend(genesis_coinbase(), 0, 41);
    BOOST_REQUIRE(archive_pool(query, conflicted));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);

    histories out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE(out.empty());

    BOOST_REQUIRE(query.pop_confirmed());
    BOOST_REQUIRE(!query.get_unconfirmed_history(cancel, out, payee_key(), max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), conflicted.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), history::rooted_height);
}

// get_confirmed_history

BOOST_AUTO_TEST_CASE(query_address__get_confirmed_history__turbo_block1a_address0__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_CHECK(!query.get_confirmed_history(cancel, cursor, out, test::block1a_address0, max_size_t, true));
    BOOST_REQUIRE_EQUAL(out.size(), 4u);

    // Identities (not part of sort).
    BOOST_CHECK_EQUAL(out.at(0).tx.hash(), test::block1a.transactions_ptr()->at(0)->hash(false)); // tx1/tx1 (same)
    BOOST_CHECK_EQUAL(out.at(1).tx.hash(), test::block2a.transactions_ptr()->at(0)->hash(false)); // tx2/tx2 (same)
    BOOST_CHECK_EQUAL(out.at(2).tx.hash(), test::block2a.transactions_ptr()->at(1)->hash(false)); // tx3/tx3 (same)
    BOOST_CHECK_EQUAL(out.at(3).tx.hash(), test::block3a.transactions_ptr()->at(0)->hash(false)); // tx6/tx6 (same)

    // Confirmed by height ascending.
    BOOST_CHECK_EQUAL(out.at(0).tx.height(), 1u);                             // tx1
    BOOST_CHECK_EQUAL(out.at(1).tx.height(), 2u);                             // tx2
    BOOST_CHECK_EQUAL(out.at(2).tx.height(), 2u);                             // tx3
    BOOST_CHECK_EQUAL(out.at(3).tx.height(), 3u);                             // tx6

    // Confirmed height by block position.
    BOOST_CHECK_EQUAL(out.at(0).position, 0u);                                // tx1
    BOOST_CHECK_EQUAL(out.at(1).position, 0u);                                // tx2
    BOOST_CHECK_EQUAL(out.at(2).position, 1u);                                // tx3
    BOOST_CHECK_EQUAL(out.at(3).position, 0u);                                // tx6

    // Fee (not part of sort).
    BOOST_CHECK_EQUAL(out.at(0).fee, history::missing_prevout);               // tx1
    BOOST_CHECK_EQUAL(out.at(1).fee, history::missing_prevout);               // tx2
    BOOST_CHECK_EQUAL(out.at(2).fee, history::missing_prevout);               // tx3
    BOOST_CHECK_EQUAL(out.at(3).fee, history::missing_prevout);               // tx6
}

BOOST_AUTO_TEST_CASE(query_address__get_confirmed_history__limited__depth_limited_empty)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching address cursor (or terminal).
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    cursor = {};
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    cursor = {};
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 9, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 4u);

    cursor = {};
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 100, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 4u);
}

BOOST_AUTO_TEST_CASE(query_address__get_confirmed_history__start__excludes_prior)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    histories out{};
    height_link cursor{ 42 };
    const std::atomic_bool cancel{};
    const auto& hash = test::block1a_address0;

    // Limited returned when limit precludes reaching address cursor (or terminal).
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 0, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    // Only unconfirmed.
    cursor = 42;
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 8, true), error::depth_limited);
    BOOST_CHECK(out.empty());

    // Only excludes genesis.
    cursor = 1;
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 9, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 4u);

    // Excludes block1a which has one confirmed touching tx.
    cursor = 2;
    BOOST_CHECK_EQUAL(query.get_confirmed_history(cancel, cursor, out, hash, 100, true), error::success);
    BOOST_CHECK_EQUAL(out.size(), 3u);
}

// get_tx_history

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::genesis.transactions_ptr()->at(0)->hash(false);
    auto history = query.get_tx_history(0);
    BOOST_CHECK(history.valid());
    BOOST_CHECK_EQUAL(history.fee, history::missing_prevout);
    BOOST_CHECK_EQUAL(history.position, 0u);
    BOOST_CHECK_EQUAL(history.tx.height(), 0u);
    BOOST_CHECK_EQUAL(history.tx.hash(), hash);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__confirmed__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::block2a.transactions_ptr()->at(0)->hash(false);
    const auto history = query.get_tx_history(query.to_tx(hash));
    BOOST_CHECK(history.valid());
    BOOST_CHECK_EQUAL(history.fee, history::missing_prevout); // spend > value
    BOOST_CHECK_EQUAL(history.position, 0u);
    BOOST_CHECK_EQUAL(history.tx.height(), 2u);
    BOOST_CHECK_EQUAL(history.tx.hash(), hash);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__confirmed_missing_prevout__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::block2a.transactions_ptr()->at(1)->hash(false);
    const auto history = query.get_tx_history(query.to_tx(hash));
    BOOST_CHECK(history.valid());
    BOOST_CHECK_EQUAL(history.fee, history::missing_prevout); // missing prevout
    BOOST_CHECK_EQUAL(history.position, 1u);
    BOOST_CHECK_EQUAL(history.tx.height(), 2u);
    BOOST_CHECK_EQUAL(history.tx.hash(), hash);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__unconfirmable__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    // tx4 is not pooled and conflicts confirmed spends (unconfirmable).
    const auto hash = test::tx4.hash(false);
    const auto history = query.get_tx_history(query.to_tx(hash));
    BOOST_CHECK(!history.valid());
    BOOST_CHECK(!history.fault());
    BOOST_CHECK_EQUAL(history.position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__unrooted__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));
    BOOST_CHECK(pool_block2b(query));

    using namespace system;
    const auto hash = test::block2b.transactions_ptr()->at(0)->hash(false);
    const auto history = query.get_tx_history(query.to_tx(hash));
    BOOST_CHECK(history.valid());
    BOOST_CHECK_EQUAL(history.fee, floored_subtract(0xb1u + 0xb1u, 0xb2u));
    BOOST_CHECK_EQUAL(history.position, history::unconfirmed_position);
    BOOST_CHECK_EQUAL(history.tx.height(), history::unrooted_height);
    BOOST_CHECK_EQUAL(history.tx.hash(), hash);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__pooled_rooted__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(archive_pool(query, tx));

    const auto history = query.get_tx_history(query.to_tx(tx.hash(false)));
    BOOST_REQUIRE(history.valid());
    BOOST_REQUIRE_EQUAL(history.tx.hash(), tx.hash(false));
    BOOST_REQUIRE_EQUAL(history.tx.height(), history::rooted_height);
    BOOST_REQUIRE_EQUAL(history.position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__unpooled__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto tx = to_spend(genesis_coinbase(), 0, 42);
    BOOST_REQUIRE(query.set(tx));

    const auto link = query.to_tx(tx.hash(false));
    BOOST_REQUIRE(query.is_confirmable_tx(link));

    const auto entry = query.get_tx_history(link);
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
    BOOST_REQUIRE_EQUAL(entry.position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__conflicted__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto conflict = to_spend(genesis_coinbase(), 0, 41);
    BOOST_REQUIRE(archive_pool(query, conflicted));

    const auto link = query.to_tx(conflicted.hash(false));
    BOOST_REQUIRE(query.get_tx_history(link).valid());
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);
    BOOST_REQUIRE(query.is_pooled(link));

    const auto entry = query.get_tx_history(link);
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
    BOOST_REQUIRE_EQUAL(entry.position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__dead_chain__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto child = to_spend(root.hash(false), 0, 41);
    const auto grandchild = to_spend(child.hash(false), 0, 40, payee());
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(archive_pool(query, grandchild));

    const auto link = query.to_tx(grandchild.hash(false));
    BOOST_REQUIRE(query.get_tx_history(link).valid());

    const auto conflict = to_spend(genesis_coinbase(), 0, 39);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);

    const auto entry = query.get_tx_history(link);
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
    BOOST_REQUIRE_EQUAL(entry.position, history::unconfirmed_position);
}

// get_tx_unconfirmed_history

BOOST_AUTO_TEST_CASE(query_address__get_tx_unconfirmed_history__unpooled__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto tx = to_spend(genesis_coinbase(), 0, 42);
    BOOST_REQUIRE(query.set(tx));

    const auto link = query.to_tx(tx.hash(false));
    BOOST_REQUIRE(query.is_confirmable_tx(link));

    const auto entry = query.get_tx_unconfirmed_history(link);
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
    BOOST_REQUIRE_EQUAL(entry.position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_unconfirmed_history__conflicted__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42, payee());
    const auto conflict = to_spend(genesis_coinbase(), 0, 41);
    BOOST_REQUIRE(archive_pool(query, conflicted));

    const auto link = query.to_tx(conflicted.hash(false));
    BOOST_REQUIRE(query.get_tx_unconfirmed_history(link).valid());
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);
    BOOST_REQUIRE(query.is_pooled(link));

    const auto entry = query.get_tx_unconfirmed_history(link);
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
    BOOST_REQUIRE_EQUAL(entry.position, history::unconfirmed_position);
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_unconfirmed_history__dead_chain__unconfirmed_position)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto child = to_spend(root.hash(false), 0, 41);
    const auto grandchild = to_spend(child.hash(false), 0, 40, payee());
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(archive_pool(query, grandchild));

    const auto link = query.to_tx(grandchild.hash(false));
    BOOST_REQUIRE(query.get_tx_unconfirmed_history(link).valid());

    const auto conflict = to_spend(genesis_coinbase(), 0, 39);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { conflict }) != system::null_hash);

    const auto entry = query.get_tx_unconfirmed_history(link);
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
    BOOST_REQUIRE_EQUAL(entry.position, history::unconfirmed_position);
}

// get_spenders_history1
// get_spenders_history2

BOOST_AUTO_TEST_CASE(query_address__get_spenders_history__bogus__empty)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    auto histories = query.get_spenders_history({ 0x42 }, 0);
    BOOST_CHECK(histories.empty());

    histories = query.get_spenders_history({ { 0x42 }, 0 });
    BOOST_CHECK(histories.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_spenders_history__genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::genesis.transactions_ptr()->at(0)->hash(false);
    auto histories = query.get_spenders_history(hash, 0);
    BOOST_CHECK(histories.empty());

    histories = query.get_spenders_history({ hash, 0 });
    BOOST_CHECK(histories.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_spenders_history__confirmed__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(test::setup_three_block_confirmed_address_store(query));

    const auto hash = test::block1a.transactions_ptr()->at(0)->hash(false);
    const auto histories = query.get_spenders_history(hash, 0);

    // tx4/tx5 are not pooled (and are unconfirmable).
    BOOST_CHECK_EQUAL(histories.size(), 2u);

    BOOST_CHECK(histories.at(0).valid());
    BOOST_CHECK_EQUAL(histories.at(0).fee, history::missing_prevout); // spend > value
    BOOST_CHECK_EQUAL(histories.at(0).position, 0u);
    BOOST_CHECK_EQUAL(histories.at(0).tx.height(), 2u);
    BOOST_CHECK_EQUAL(histories.at(0).tx.hash(), test::block2a.transactions_ptr()->at(0)->hash(false));

    BOOST_CHECK(histories.at(1).valid());
    BOOST_CHECK_EQUAL(histories.at(1).fee, history::missing_prevout); // spend > value
    BOOST_CHECK_EQUAL(histories.at(1).position, 0u);
    BOOST_CHECK_EQUAL(histories.at(1).tx.height(), 3u);
    BOOST_CHECK_EQUAL(histories.at(1).tx.hash(), test::block3a.transactions_ptr()->at(0)->hash(false));
}

BOOST_AUTO_TEST_CASE(query_address__get_spenders_history__conflicted_spender__excluded)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto conflicted = to_spend(genesis_coinbase(), 0, 42);
    const auto replacement = to_spend(genesis_coinbase(), 0, 41);
    BOOST_REQUIRE(archive_pool(query, conflicted));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);

    const auto spenders = query.get_spenders_history({ genesis_coinbase(), 0 });
    BOOST_REQUIRE_EQUAL(spenders.size(), 1u);
    BOOST_REQUIRE_EQUAL(spenders.at(0).tx.hash(), replacement.hash(false));
    BOOST_REQUIRE_EQUAL(spenders.at(0).tx.height(), 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_spenders_history__dead_spenders__live_only)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto funder = to_spend(system::one_hash, 0, 10);
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { funder });
    BOOST_REQUIRE(block1 != system::null_hash);

    // Spenders of the funder, each also spending an output of one dead ancestry.
    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto fan = to_fan(root.hash(false), 0, 3);
    const auto first = to_spend({ { funder.hash(false), 0 }, { fan.hash(false), 0 } }, 9);
    const auto second = to_spend({ { funder.hash(false), 0 }, { fan.hash(false), 1 } }, 8);
    const auto third = to_spend({ { funder.hash(false), 0 }, { fan.hash(false), 2 } }, 7);
    const auto live = to_spend(funder.hash(false), 0, 6);
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, fan));
    BOOST_REQUIRE(archive_pool(query, first));
    BOOST_REQUIRE(archive_pool(query, second));
    BOOST_REQUIRE(archive_pool(query, third));
    BOOST_REQUIRE(archive_pool(query, live));
    BOOST_REQUIRE_EQUAL(query.get_spenders_history({ funder.hash(false), 0 }).size(), 4u);

    const auto conflict = to_spend(genesis_coinbase(), 0, 39);
    BOOST_REQUIRE(archive_confirm(query, block1, 2, { conflict }) != system::null_hash);

    const auto spenders = query.get_spenders_history({ funder.hash(false), 0 });
    BOOST_REQUIRE_EQUAL(spenders.size(), 1u);
    BOOST_REQUIRE_EQUAL(spenders.at(0).tx.hash(), live.hash(false));
    BOOST_REQUIRE_EQUAL(spenders.at(0).tx.height(), history::rooted_height);
}

// duplicate instances (one tx archived twice, only one instance associated)

BOOST_AUTO_TEST_CASE(query_address__get_tx_history__duplicate_instances__same_entry)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(!store.is_pooling());
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE(query.push_confirmed(1, false));
    BOOST_REQUIRE(query.set(test::tx4));
    const auto loose = query.to_tx(test::tx4.hash(false));
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE(query.push_confirmed(2, false));
    const auto stored = query.get_position_tx(query.to_header(test::block_spend_1a.hash()), 0);
    BOOST_REQUIRE(stored != loose);

    const auto expected = query.get_tx_history(stored);
    BOOST_REQUIRE(expected.valid());
    BOOST_REQUIRE(expected.confirmed());
    BOOST_REQUIRE_EQUAL(expected.tx.height(), 2u);
    BOOST_REQUIRE_EQUAL(expected.position, 0u);

    const auto duplicate = query.get_tx_history(loose);
    BOOST_REQUIRE(!duplicate.fault());
    BOOST_REQUIRE(duplicate.valid());
    BOOST_REQUIRE(duplicate == expected);
    BOOST_REQUIRE_EQUAL(duplicate.tx.height(), 2u);
    BOOST_REQUIRE_EQUAL(duplicate.position, 0u);

    const auto confirmed = query.get_tx_confirmed_history(loose);
    BOOST_REQUIRE(!confirmed.fault());
    BOOST_REQUIRE(confirmed == expected);
}

BOOST_AUTO_TEST_CASE(query_address__get_history__duplicate_instances__deduplicated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(!store.is_pooling());
    BOOST_REQUIRE(query.initialize(test::genesis));
    BOOST_REQUIRE(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(1));
    BOOST_REQUIRE(query.push_confirmed(1, false));
    BOOST_REQUIRE(query.set(test::tx4));
    BOOST_REQUIRE(query.set(test::block_spend_1a, context{ 0, 2, 0 }, {}, false, false));
    BOOST_REQUIRE(query.set_strong(2));
    BOOST_REQUIRE(query.push_confirmed(2, false));
    BOOST_REQUIRE_EQUAL(query.to_duplicates(test::tx4.hash(false)).size(), 2u);

    histories out{};
    height_link cursor{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE_EQUAL(query.get_history(cancel, cursor, out, test::block1a_address0, max_size_t, true), error::success);

    // block1a coinbase (funds address0) and tx4 (spends it, pays address0), once each.
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    BOOST_REQUIRE_EQUAL(out.at(0).tx.hash(), test::block1a.transactions_ptr()->at(0)->hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).tx.height(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(1).tx.hash(), test::tx4.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(1).tx.height(), 2u);
    BOOST_REQUIRE_EQUAL(out.at(1).position, 0u);

    histories confirmed{};
    height_link confirmed_cursor{};
    BOOST_REQUIRE_EQUAL(query.get_confirmed_history(cancel, confirmed_cursor, confirmed, test::block1a_address0, max_size_t, true), error::success);
    BOOST_REQUIRE_EQUAL(confirmed.size(), 2u);
    BOOST_REQUIRE_EQUAL(confirmed.at(1).tx.hash(), test::tx4.hash(false));
}

BOOST_AUTO_TEST_SUITE_END()
