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

BOOST_FIXTURE_TEST_SUITE(query_consensus_tests, test::directory_setup_fixture)

using namespace system::chain;
using context = database::context;
constexpr auto bip113 = flags::bip113_rule;

// populate_pooled

BOOST_AUTO_TEST_CASE(query_consensus__populate_pooled__sufficient__external_metadata)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const transaction source{ test::tx_spend_one_hash.to_data(true), true };
    const auto& source_in = *source.inputs_ptr()->front();
    source_in.prevout = system::to_shared<output>(0x30, script{});
    source_in.metadata.parent_tx = 42;
    source_in.metadata.coinbase = true;
    BOOST_REQUIRE(query.set_pooled(1, source, context{ bip113, 8, 9 }, 0));

    const transaction tx{ test::tx_spend_one_hash.to_data(true), true };
    const auto& in = *tx.inputs_ptr()->front();

    pooled_tx pooled{};
    BOOST_REQUIRE_EQUAL(query.populate_pooled(pooled, tx, 1, context{ bip113, 9, 10 }), error::success);
    BOOST_REQUIRE_EQUAL(pooled.fee, 0x20u);
    BOOST_REQUIRE_EQUAL(in.metadata.parent_tx, 42u);
    BOOST_REQUIRE(in.metadata.coinbase);
    BOOST_REQUIRE(!in.prevout);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_pooled__internal_spend__terminal_metadata)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const transaction source{ test::tx_spend_one_hash.to_data(true), true };
    const auto& source_in = *source.inputs_ptr()->front();
    source_in.prevout = system::to_shared<output>(0x30, script{});
    source_in.metadata.parent_tx = 42;
    source_in.metadata.coinbase = true;
    BOOST_REQUIRE(query.set_pooled(1, source, context{ bip113, 8, 9 }, 0));

    const transaction tx{ test::tx_spend_one_hash.to_data(true), true };
    const auto& in = *tx.inputs_ptr()->front();
    in.prevout = system::to_shared<output>(0x30, script{});
    in.metadata.coinbase = false;

    pooled_tx pooled{};
    BOOST_REQUIRE_EQUAL(query.populate_pooled(pooled, tx, 1, context{ bip113, 8, 9 }), error::success);
    BOOST_REQUIRE_EQUAL(in.metadata.parent_tx, max_uint32);
    BOOST_REQUIRE(!in.metadata.coinbase);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_pooled__insufficient__unvalidated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const transaction source{ test::tx_spend_one_hash.to_data(true), true };
    const auto& source_in = *source.inputs_ptr()->front();
    source_in.prevout = system::to_shared<output>(0x30, script{});
    source_in.metadata.parent_tx = 42;
    BOOST_REQUIRE(query.set_pooled(1, source, context{ bip113, 8, 9 }, 0));

    const transaction tx{ test::tx_spend_one_hash.to_data(true), true };
    const auto& in = *tx.inputs_ptr()->front();

    pooled_tx pooled{};
    BOOST_REQUIRE_EQUAL(query.populate_pooled(pooled, tx, 1, context{ bip113, 7, 9 }), error::unvalidated);
    BOOST_REQUIRE_EQUAL(in.metadata.parent_tx, max_uint32);
}

// populate_with_metadata
// ----------------------------------------------------------------------------

// METADATA IS SETTABLE ON CONST TEST OBJECTS.
// COPY CONSTRUCTION CREATES SAHRED POINTER REFERENCES.
// SO POPULATE ON CONST OBJECTS HAS SIDE EFFECTS IF THE OBJECTS SPAN TESTS.
const auto& clean_(const auto& block_or_tx) NOEXCEPT
{
    const auto inputs = block_or_tx.inputs_ptr();
    for (const auto& input : *inputs)
    {
        input->prevout.reset();
        input->metadata = system::chain::prevout{};
    }

    return block_or_tx;
}

// First four blocks have only coinbase txs.
BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__null_prevouts__true)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block2, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block3, test::context, {}, false, false));

    const auto& copy = clean_(test::genesis);
    const auto& copy1 = clean_(test::block1);
    const auto& copy2 = clean_(test::block2);
    const auto& copy3 = clean_(test::block3);

    BOOST_CHECK(query.populate_with_metadata(copy));
    BOOST_CHECK(query.populate_with_metadata(copy1));
    BOOST_CHECK(query.populate_with_metadata(copy2));
    BOOST_CHECK(query.populate_with_metadata(copy3));
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__partial_prevouts__false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1a, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block2a, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::tx4));

    const auto& block1a = clean_(test::block1a);

    // Block populate treates first tx as null point.
    BOOST_CHECK(query.populate_with_metadata(block1a));
    BOOST_CHECK(!query.populate_with_metadata(*block1a.transactions_ptr()->at(0)));
    BOOST_CHECK(!query.populate_with_metadata(*block1a.inputs_ptr()->at(0)));
    BOOST_CHECK(!query.populate_with_metadata(*block1a.inputs_ptr()->at(2)));

    // Block populate treates first tx as null point and other has missing prevouts.
    const auto& block2a = clean_(test::block2a);
    BOOST_CHECK(!query.populate_with_metadata(block2a));
    BOOST_CHECK(query.populate_with_metadata(*block2a.transactions_ptr()->at(0)));
    BOOST_CHECK(!query.populate_with_metadata(*block2a.transactions_ptr()->at(1)));
    BOOST_CHECK(query.populate_with_metadata(*block2a.inputs_ptr()->at(0)));
    BOOST_CHECK(!query.populate_with_metadata(*block2a.inputs_ptr()->at(3)));

    const auto& tx4 = clean_(test::tx4);
    BOOST_CHECK(query.populate_with_metadata(tx4));
    BOOST_CHECK(query.populate_with_metadata(*tx4.inputs_ptr()->at(0)));
    BOOST_CHECK(query.populate_with_metadata(*tx4.inputs_ptr()->at(1)));
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__metadata__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1a, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block2a, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::tx4));
    BOOST_CHECK(!query.is_coinbase(1));

    // Genesis only has coinbase, which does not spend.
    const auto& genesis = clean_(test::genesis);

    BOOST_CHECK(!genesis.inputs_ptr()->at(0)->prevout);
    BOOST_CHECK(genesis.inputs_ptr()->at(0)->metadata.coinbase);
    BOOST_CHECK_EQUAL(genesis.inputs_ptr()->at(0)->metadata.parent_tx, max_uint32);

    BOOST_CHECK(query.populate_with_metadata(genesis));

    BOOST_CHECK(!genesis.inputs_ptr()->at(0)->prevout);
    BOOST_CHECK(genesis.inputs_ptr()->at(0)->metadata.coinbase);
    BOOST_CHECK_EQUAL(genesis.inputs_ptr()->at(0)->metadata.parent_tx, max_uint32);

    // Transaction population.
    const auto& tx4 = clean_(test::tx4);

    BOOST_CHECK(!tx4.inputs_ptr()->at(0)->prevout);
    BOOST_CHECK(tx4.inputs_ptr()->at(0)->metadata.coinbase);
    BOOST_CHECK_EQUAL(tx4.inputs_ptr()->at(0)->metadata.parent_tx, max_uint32);
    BOOST_CHECK(!tx4.inputs_ptr()->at(1)->prevout);
    BOOST_CHECK(tx4.inputs_ptr()->at(1)->metadata.coinbase);
    BOOST_CHECK_EQUAL(tx4.inputs_ptr()->at(1)->metadata.parent_tx, max_uint32);

    BOOST_CHECK(query.populate_with_metadata(tx4));

    // TODO: test non-coinbase and other parent.
    // spent/mtp are defaults, coinbase/parent are set (to non-default values).
    BOOST_CHECK(tx4.inputs_ptr()->at(0)->prevout);
    BOOST_CHECK(!tx4.inputs_ptr()->at(0)->metadata.coinbase);
    BOOST_CHECK_EQUAL(tx4.inputs_ptr()->at(0)->metadata.parent_tx, 1u);
    BOOST_CHECK(tx4.inputs_ptr()->at(1)->prevout);
    BOOST_CHECK(!tx4.inputs_ptr()->at(1)->metadata.coinbase);
    BOOST_CHECK_EQUAL(tx4.inputs_ptr()->at(1)->metadata.parent_tx, 1u);
}

// A free tx conflicts with a pooled and confirmable spender, but only a
// confirmed one has height.
BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_spender__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1a, context{ 0, 1, 0 }, {}, false, false));

    // tx5 spends the first output of the first tx of block1a, as do tx4 and
    // the first transaction of block2a.
    const auto& unspent = clean_(test::tx5);
    BOOST_CHECK(query.populate_with_metadata(unspent, true, true));
    BOOST_CHECK_EQUAL(unspent.inputs_ptr()->at(0)->metadata.spender_height, max_uint32);

    BOOST_CHECK(query.set(test::tx4));

    // The spender is archived but not pooled, so it is not a conflict.
    const auto& archived = clean_(test::tx5);
    BOOST_CHECK(query.populate_with_metadata(archived, true, true));
    BOOST_CHECK_EQUAL(archived.inputs_ptr()->at(0)->metadata.spender_height, max_uint32);

    const transaction spender{ test::tx4.to_data(true), true };
    BOOST_CHECK(query.set_pooled(query.to_tx(test::tx4.hash(false)), spender, context{ bip113, 8, 9 }, 0));

    const auto& pooled = clean_(test::tx5);
    BOOST_CHECK(query.populate_with_metadata(pooled, true, true));
    BOOST_CHECK_EQUAL(pooled.inputs_ptr()->at(0)->metadata.spender_height, 0u);

    // The spender is unassociated, so it is not a confirmed spender.
    const auto& chained = clean_(test::tx5);
    BOOST_CHECK(query.populate_with_metadata(chained, true, false));
    BOOST_CHECK_EQUAL(chained.inputs_ptr()->at(0)->metadata.spender_height, max_uint32);

    BOOST_CHECK(query.set(test::block2a, context{ 0, 2, 0 }, {}, false, false));
    BOOST_CHECK(query.set_strong(2));

    const auto& confirmed = clean_(test::tx5);
    BOOST_CHECK(query.populate_with_metadata(confirmed, true, true));
    BOOST_CHECK_EQUAL(confirmed.inputs_ptr()->at(0)->metadata.spender_height, 2u);

    const auto& strong = clean_(test::tx5);
    BOOST_CHECK(query.populate_with_metadata(strong, true, false));
    BOOST_CHECK_EQUAL(strong.inputs_ptr()->at(0)->metadata.spender_height, 2u);
}

static const system::hash_digest& genesis_coinbase() NOEXCEPT
{
    static const auto hash =
        test::genesis.transactions_ptr()->front()->hash(false);
    return hash;
}

// Two outputs of value, so that another tx may spend the second (index 1).
static transaction to_spend(const system::hash_digest& hash, uint32_t index,
    uint64_t value) NOEXCEPT
{
    const input input{ point{ hash, index }, script{}, max_uint32 };
    return { 1u, inputs{ input }, outputs{ output{ value, script{} },
        output{ value, script{} } }, 0u };
}

static transaction to_spend(const points& points, uint64_t value) NOEXCEPT
{
    inputs ins{};
    for (const auto& point: points)
        ins.emplace_back(point, script{}, max_uint32);

    return { 1u, std::move(ins), outputs{ output{ value, script{} } }, 0u };
}

static transaction to_fan(const system::hash_digest& hash, uint32_t index,
    size_t count) NOEXCEPT
{
    const input input{ point{ hash, index }, script{}, max_uint32 };
    return { 1u, inputs{ input }, outputs(count, output{ 1u, script{} }), 0u };
}

// Populated for the pooled fee, archived and pooled (parents archived).
static bool archive_pool(test::query_accessor& query,
    const transaction& tx) NOEXCEPT
{
    const auto stored = query.populate_with_metadata(tx) && query.set(tx);
    const auto link = query.to_tx(tx.hash(false));
    return stored && query.set_pooled(link, tx, context{ bip113, 8, 9 }, 0);
}

// Confirm a block of a coinbase and the txs at height above parent.
static system::hash_digest archive_confirm(test::query_accessor& query,
    const system::hash_digest& parent, uint32_t height,
    const transactions& txs) NOEXCEPT
{
    const input null{ point{ system::null_hash, point::null_index }, script{},
        max_uint32 };
    transactions block_txs{ transaction{ 1u, inputs{ null },
        outputs{ output{ 42u, script{} } }, height } };
    block_txs.insert(block_txs.end(), txs.cbegin(), txs.cend());
    const block block{ header{ 1u, parent, system::null_hash, 1u, 2u, height },
        block_txs };
    const auto stored = query.set(block, context{ 0, height, 0 }, {}, false,
        true);
    const auto link = query.to_header(block.hash());
    const auto confirmed = stored && query.push_candidate(link) &&
        query.push_confirmed(link, true);
    return confirmed ? block.hash() : system::null_hash;
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_conflicted_spender__unspent)
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

    // The spender is live (pooled and confirmable), then its other parent is
    // conflicted by a confirmed tx, so the spender can no longer confirm.
    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    const auto spender = to_spend({ { parent.hash(false), 0 }, { funder.hash(false), 0 } }, 50);
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(archive_pool(query, spender));

    const auto live = to_spend(funder.hash(false), 0, 9);
    BOOST_REQUIRE(query.populate_with_metadata(live, true, true));
    BOOST_REQUIRE_EQUAL(live.inputs_ptr()->front()->metadata.spender_height, 0u);

    BOOST_REQUIRE(archive_confirm(query, block1, 2, { to_spend(genesis_coinbase(), 0, 40) }) != system::null_hash);

    const auto dead = to_spend(funder.hash(false), 0, 9);
    BOOST_REQUIRE(query.populate_with_metadata(dead, true, true));
    BOOST_REQUIRE_EQUAL(dead.inputs_ptr()->front()->metadata.spender_height, max_uint32);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_unpooled_spender__unspent)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto funder = to_spend(system::one_hash, 0, 10);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { funder }) != system::null_hash);

    const auto spender = to_spend(funder.hash(false), 0, 8);
    BOOST_REQUIRE(query.set(spender));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(spender.hash(false))));
    BOOST_REQUIRE(query.is_confirmable_tx(query.to_tx(spender.hash(false))));

    const auto respend = to_spend(funder.hash(false), 0, 9);
    BOOST_REQUIRE(query.populate_with_metadata(respend, true, true));
    BOOST_REQUIRE_EQUAL(respend.inputs_ptr()->front()->metadata.spender_height, max_uint32);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_conflicted_parent__false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { to_spend(genesis_coinbase(), 0, 41) }) != system::null_hash);

    // Block (not pool) population is unchanged.
    const auto confirming = to_spend(parent.hash(false), 0, 40);
    BOOST_REQUIRE(query.populate_with_metadata(confirming, true, false));

    const auto child = to_spend(parent.hash(false), 0, 40);
    BOOST_REQUIRE(!query.populate_with_metadata(child, true, true));
    BOOST_REQUIRE(!child.inputs_ptr()->front()->prevout);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_conflicted_chain__false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // A pooled chain whose root is then conflicted by a confirmed tx.
    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto child = to_spend(root.hash(false), 0, 41);
    const auto grandchild = to_spend(child.hash(false), 0, 40);
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(archive_pool(query, grandchild));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { to_spend(genesis_coinbase(), 0, 39) }) != system::null_hash);

    // Each input's parent is unconfirmable.
    const auto spender = to_spend({ { grandchild.hash(false), 1 }, { child.hash(false), 1 }, { root.hash(false), 1 } }, 30);
    BOOST_REQUIRE(!query.populate_with_metadata(spender, true, true));
    BOOST_REQUIRE(!spender.inputs_ptr()->at(0)->prevout);
    BOOST_REQUIRE(!query.populate_with_metadata(*spender.inputs_ptr()->at(1), true, true));
    BOOST_REQUIRE(!query.populate_with_metadata(*spender.inputs_ptr()->at(2), true, true));
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_dead_fanout_parents__false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Each parent descends from one pooled chain, which is then conflicted.
    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto middle = to_spend(root.hash(false), 0, 41);
    const auto fan = to_fan(middle.hash(false), 0, 3);
    const auto first = to_spend({ { fan.hash(false), 0 } }, 1);
    const auto second = to_spend({ { fan.hash(false), 1 } }, 1);
    const auto third = to_spend({ { fan.hash(false), 2 } }, 1);
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, middle));
    BOOST_REQUIRE(archive_pool(query, fan));
    BOOST_REQUIRE(archive_pool(query, first));
    BOOST_REQUIRE(archive_pool(query, second));
    BOOST_REQUIRE(archive_pool(query, third));

    const auto live = to_spend({ { first.hash(false), 0 }, { second.hash(false), 0 }, { third.hash(false), 0 } }, 2);
    BOOST_REQUIRE(query.populate_with_metadata(live, true, true));

    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { to_spend(genesis_coinbase(), 0, 39) }) != system::null_hash);

    const auto dead = to_spend({ { first.hash(false), 0 }, { second.hash(false), 0 }, { third.hash(false), 0 } }, 2);
    const auto& ins = *dead.inputs_ptr();
    BOOST_REQUIRE(!query.populate_with_metadata(dead, true, true));
    BOOST_REQUIRE(!ins.at(0)->prevout);
    BOOST_REQUIRE(!query.populate_with_metadata(*ins.at(1), true, true));
    BOOST_REQUIRE(!query.populate_with_metadata(*ins.at(2), true, true));
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_dead_fanout_spenders__unspent)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto funder = to_fan(system::one_hash, 0, 3);
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { funder });
    BOOST_REQUIRE(block1 != system::null_hash);

    // Each spender of a funder output also spends an output of one pooled
    // chain, which is then conflicted.
    const auto root = to_spend(genesis_coinbase(), 0, 42);
    const auto middle = to_spend(root.hash(false), 0, 41);
    const auto fan = to_fan(middle.hash(false), 0, 3);
    const auto first = to_spend({ { funder.hash(false), 0 }, { fan.hash(false), 0 } }, 2);
    const auto second = to_spend({ { funder.hash(false), 1 }, { fan.hash(false), 1 } }, 2);
    const auto third = to_spend({ { funder.hash(false), 2 }, { fan.hash(false), 2 } }, 2);
    BOOST_REQUIRE(archive_pool(query, root));
    BOOST_REQUIRE(archive_pool(query, middle));
    BOOST_REQUIRE(archive_pool(query, fan));
    BOOST_REQUIRE(archive_pool(query, first));
    BOOST_REQUIRE(archive_pool(query, second));
    BOOST_REQUIRE(archive_pool(query, third));

    const auto live = to_spend({ { funder.hash(false), 0 }, { funder.hash(false), 1 }, { funder.hash(false), 2 } }, 2);
    const auto& live_ins = *live.inputs_ptr();
    BOOST_REQUIRE(query.populate_with_metadata(live, true, true));
    BOOST_REQUIRE_EQUAL(live_ins.at(0)->metadata.spender_height, 0u);
    BOOST_REQUIRE_EQUAL(live_ins.at(1)->metadata.spender_height, 0u);
    BOOST_REQUIRE_EQUAL(live_ins.at(2)->metadata.spender_height, 0u);

    BOOST_REQUIRE(archive_confirm(query, block1, 2, { to_spend(genesis_coinbase(), 0, 39) }) != system::null_hash);

    const auto dead = to_spend({ { funder.hash(false), 0 }, { funder.hash(false), 1 }, { funder.hash(false), 2 } }, 2);
    const auto& dead_ins = *dead.inputs_ptr();
    BOOST_REQUIRE(query.populate_with_metadata(dead, true, true));
    BOOST_REQUIRE_EQUAL(dead_ins.at(0)->metadata.spender_height, max_uint32);
    BOOST_REQUIRE_EQUAL(dead_ins.at(1)->metadata.spender_height, max_uint32);
    BOOST_REQUIRE_EQUAL(dead_ins.at(2)->metadata.spender_height, max_uint32);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_unpooled_parent__populated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // An unconfirmed parent must be confirmable, not pooled (for example a tx
    // of a block archived but not yet confirmed).
    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    BOOST_REQUIRE(query.set(parent));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(parent.hash(false))));
    BOOST_REQUIRE(query.is_confirmable_tx(query.to_tx(parent.hash(false))));

    const auto child = to_spend(parent.hash(false), 0, 41);
    BOOST_REQUIRE(query.populate_with_metadata(child, true, true));
    BOOST_REQUIRE_EQUAL(child.inputs_ptr()->front()->metadata.spender_height, max_uint32);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_disabled_spender__conflict)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    settings.pool.buckets = 0;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto funder = to_spend(system::one_hash, 0, 10);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { funder }) != system::null_hash);

    // Without a pool table any archived spender is a conflict.
    const auto spender = to_spend(funder.hash(false), 0, 8);
    BOOST_REQUIRE(query.set(spender));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(spender.hash(false))));

    const auto respend = to_spend(funder.hash(false), 0, 9);
    BOOST_REQUIRE(query.populate_with_metadata(respend, true, true));
    BOOST_REQUIRE_EQUAL(respend.inputs_ptr()->front()->metadata.spender_height, 0u);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_disabled_parent__populated)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    settings.pool.buckets = 0;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    BOOST_REQUIRE(query.set(parent));
    BOOST_REQUIRE(!query.is_pooled(query.to_tx(parent.hash(false))));

    const auto child = to_spend(parent.hash(false), 0, 41);
    BOOST_REQUIRE(query.populate_with_metadata(child, true, true));
    BOOST_REQUIRE_EQUAL(child.inputs_ptr()->front()->metadata.spender_height, max_uint32);
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_with_metadata__pool_disabled_conflicted_parent__false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    settings.pool.buckets = 0;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));

    // Without a pool table an unconfirmed prevout must still be confirmable.
    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    BOOST_REQUIRE(query.set(parent));
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { to_spend(genesis_coinbase(), 0, 41) }) != system::null_hash);

    const auto child = to_spend(parent.hash(false), 0, 40);
    BOOST_REQUIRE(!query.populate_with_metadata(child, true, true));
    BOOST_REQUIRE(!child.inputs_ptr()->front()->prevout);
}

// populate_without_metadata
// ----------------------------------------------------------------------------

// First four blocks have only coinbase txs.
BOOST_AUTO_TEST_CASE(query_consensus__populate_without_metadata__null_prevouts__true)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block2, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block3, test::context, {}, false, false));
    BOOST_CHECK(query.populate_without_metadata(test::genesis));
    BOOST_CHECK(query.populate_without_metadata(test::block1));
    BOOST_CHECK(query.populate_without_metadata(test::block2));
    BOOST_CHECK(query.populate_without_metadata(test::block3));
}

BOOST_AUTO_TEST_CASE(query_consensus__populate_without_metadata__partial_prevouts__false)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_CHECK(!store.create(test::events_handler));
    BOOST_CHECK(query.initialize(test::genesis));
    BOOST_CHECK(query.set(test::block1a, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::block2a, test::context, {}, false, false));
    BOOST_CHECK(query.set(test::tx4));

    // Block populate treates first tx as null point.
    BOOST_CHECK(query.populate_without_metadata(test::block1a));
    BOOST_CHECK(!query.populate_without_metadata(*test::block1a.transactions_ptr()->at(0)));
    BOOST_CHECK(!query.populate_without_metadata(*test::block1a.inputs_ptr()->at(0)));
    BOOST_CHECK(!query.populate_without_metadata(*test::block1a.inputs_ptr()->at(2)));

    // Block populate treates first tx as null point and other has missing prevouts.
    BOOST_CHECK(!query.populate_without_metadata(test::block2a));
    BOOST_CHECK(query.populate_without_metadata(*test::block2a.transactions_ptr()->at(0)));
    BOOST_CHECK(!query.populate_without_metadata(*test::block2a.transactions_ptr()->at(1)));
    BOOST_CHECK(query.populate_without_metadata(*test::block2a.inputs_ptr()->at(0)));
    BOOST_CHECK(!query.populate_without_metadata(*test::block2a.inputs_ptr()->at(3)));

    BOOST_CHECK(query.populate_without_metadata(test::tx4));
    BOOST_CHECK(query.populate_without_metadata(*test::tx4.inputs_ptr()->at(0)));
    BOOST_CHECK(query.populate_without_metadata(*test::tx4.inputs_ptr()->at(1)));
}

BOOST_AUTO_TEST_SUITE_END()
