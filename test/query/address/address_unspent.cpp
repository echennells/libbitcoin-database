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

// get_unconfirmed_unspent
// get_confirmed_unspent
// get_unspent

BOOST_AUTO_TEST_CASE(query_address__get_unspent__genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent(cancel, out, test::genesis_address0));
    BOOST_REQUIRE_EQUAL(out.size(), 0u);

    out.clear();
    BOOST_REQUIRE(!query.get_confirmed_unspent(cancel, out, test::genesis_address0));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).height, 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.value(), 5'000'000'000u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().index(), 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), test::genesis.transactions_ptr()->at(0)->hash(false));

    out.clear();
    BOOST_REQUIRE(!query.get_unspent(cancel, out, test::genesis_address0));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).height, 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.value(), 5'000'000'000u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().index(), 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), test::genesis.transactions_ptr()->at(0)->hash(false));
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__turbo_genesis__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(test::setup_three_block_confirmed_address_store(query));

    // get_unspent() lists an unconfirmed output only if its tx is pooled.
    const auto& spend2b = *test::block2b.transactions_ptr()->at(0);
    BOOST_REQUIRE(query.set_pooled(query.to_tx(spend2b.hash(false)), spend2b, test::context, 0));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unconfirmed_unspent(cancel, out, test::block1a_address0, true));

    // tx4/5 contain one address output each.
    // block1b (unconfirmed) contains 2 address outputs, both spent by block2b (0 unspent).
    // block2b (unconfirmed) contains 1 address output, so 3 total address outputs.
    BOOST_CHECK_EQUAL(out.size(), 3u);

    BOOST_CHECK_EQUAL(out.at(0).height, unspent_output::unused_height);
    BOOST_CHECK_EQUAL(out.at(0).position, unspent_output::unconfirmed_position);
    BOOST_CHECK_EQUAL(out.at(0).out.value(), 0x08u);
    BOOST_CHECK_EQUAL(out.at(0).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(0).out.point().hash(), test::tx4.hash(false));

    BOOST_CHECK_EQUAL(out.at(1).height, unspent_output::unused_height);
    BOOST_CHECK_EQUAL(out.at(1).position, unspent_output::unconfirmed_position);
    BOOST_CHECK_EQUAL(out.at(1).out.value(), 0x85u);
    BOOST_CHECK_EQUAL(out.at(1).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(1).out.point().hash(), test::tx5.hash(false));

    BOOST_CHECK_EQUAL(out.at(2).height, unspent_output::unused_height);
    BOOST_CHECK_EQUAL(out.at(2).position, unspent_output::unconfirmed_position);
    BOOST_CHECK_EQUAL(out.at(2).out.value(), 0xb2u);
    BOOST_CHECK_EQUAL(out.at(2).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(2).out.point().hash(), test::block2b.transactions_ptr()->at(0)->hash(false));

    // Arbitrary but consistent sort based on chain::point::operator<.
    BOOST_CHECK(out.at(0).out < out.at(1).out);
    BOOST_CHECK(out.at(1).out < out.at(2).out);

    out.clear();
    BOOST_REQUIRE(!query.get_confirmed_unspent(cancel, out, test::block1a_address0, true));

    // block1a is confirmed spent by block2a, block2a/3a are confirmed and contain 3 unspent.
    BOOST_CHECK_EQUAL(out.size(), 3u);

    BOOST_CHECK_EQUAL(out.at(0).height, 2u);
    BOOST_CHECK_EQUAL(out.at(0).position, 0u);
    BOOST_CHECK_EQUAL(out.at(0).out.value(), 0x81u);
    BOOST_CHECK_EQUAL(out.at(0).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(0).out.point().hash(), test::block2a.transactions_ptr()->at(0)->hash(false));

    BOOST_CHECK_EQUAL(out.at(1).height, 2u);
    BOOST_CHECK_EQUAL(out.at(1).position, 1u);
    BOOST_CHECK_EQUAL(out.at(1).out.value(), 0x81u);
    BOOST_CHECK_EQUAL(out.at(1).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(1).out.point().hash(), test::block2a.transactions_ptr()->at(1)->hash(false));

    BOOST_CHECK_EQUAL(out.at(2).height, 3u);
    BOOST_CHECK_EQUAL(out.at(2).position, 0u);
    BOOST_CHECK_EQUAL(out.at(2).out.value(), 0x83u);
    BOOST_CHECK_EQUAL(out.at(2).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(2).out.point().hash(), test::block3a.transactions_ptr()->at(0)->hash(false));

    out.clear();
    BOOST_REQUIRE(!query.get_unspent(cancel, out, test::block1a_address0, true));

    // get_unspent() contains the 3 confirmed and 1 pooled (block2b) address
    // outputs, as tx4/5 are unpooled (and unconfirmable, block2a spends their
    // prevouts).
    BOOST_CHECK_EQUAL(out.size(), 4u);

    // same as get_unconfirmed_unspent()

    BOOST_CHECK_EQUAL(out.at(0).height, 2u);
    BOOST_CHECK_EQUAL(out.at(0).position, 0u);
    BOOST_CHECK_EQUAL(out.at(0).out.value(), 0x81u);
    BOOST_CHECK_EQUAL(out.at(0).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(0).out.point().hash(), test::block2a.transactions_ptr()->at(0)->hash(false));

    BOOST_CHECK_EQUAL(out.at(1).height, 2u);
    BOOST_CHECK_EQUAL(out.at(1).position, 1u);
    BOOST_CHECK_EQUAL(out.at(1).out.value(), 0x81u);
    BOOST_CHECK_EQUAL(out.at(1).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(1).out.point().hash(), test::block2a.transactions_ptr()->at(1)->hash(false));

    BOOST_CHECK_EQUAL(out.at(2).height, 3u);
    BOOST_CHECK_EQUAL(out.at(2).position, 0u);
    BOOST_CHECK_EQUAL(out.at(2).out.value(), 0x83u);
    BOOST_CHECK_EQUAL(out.at(2).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(2).out.point().hash(), test::block3a.transactions_ptr()->at(0)->hash(false));

    // same as get_confirmed_unspent()

    BOOST_CHECK_EQUAL(out.at(3).height, unspent_output::unused_height);
    BOOST_CHECK_EQUAL(out.at(3).position, unspent_output::unconfirmed_position);
    BOOST_CHECK_EQUAL(out.at(3).out.value(), 0xb2u);
    BOOST_CHECK_EQUAL(out.at(3).out.point().index(), 0u);
    BOOST_CHECK_EQUAL(out.at(3).out.point().hash(), test::block2b.transactions_ptr()->at(0)->hash(false));
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

BOOST_AUTO_TEST_CASE(query_address__get_unspent__unpooled__excluded)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__conflicted__excluded)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);

    // The pooled tx is unconfirmable once its prevout is confirmed spent.
    const auto replacement = to_spend(genesis_coinbase(), 0, 41);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__conflicted_chain__excluded)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), child.hash(false));

    // The child is unconfirmable by ancestry (its parent's prevout is spent).
    const auto replacement = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { replacement }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__pooled_spender__excluded)
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

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).height, unspent_output::unused_height);
    BOOST_REQUIRE_EQUAL(out.at(0).position, unspent_output::unconfirmed_position);

    const auto child = to_spend(parent.hash(false), 0, 41);
    BOOST_REQUIRE(archive_pool(query, child));
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__unpooled_spender__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto funder = to_spend(system::one_hash, 0, 10, payee());
    BOOST_REQUIRE(archive_confirm(query, test::genesis.hash(), 1, { funder }) != system::null_hash);

    // An archived spender that is not pooled is not an unconfirmed spend.
    const auto spender = to_spend(funder.hash(false), 0, 9);
    BOOST_REQUIRE(query.set(spender));
    BOOST_REQUIRE(query.is_spent(query.to_output(funder.hash(false), 0)));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), funder.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).height, 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__conflicted_spender__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto funder = to_spend(system::one_hash, 0, 10, payee());
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { funder });
    BOOST_REQUIRE(block1 != system::null_hash);

    const auto spender = to_spend({ { funder.hash(false), 0 }, { genesis_coinbase(), 0 } }, 50);
    BOOST_REQUIRE(archive_pool(query, spender));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE(out.empty());

    // The spender is unconfirmable once its other prevout is confirmed spent.
    const auto replacement = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_confirm(query, block1, 2, { replacement }) != system::null_hash);
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), funder.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).height, 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__conflicted_parent_spender__included)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto funder = to_spend(system::one_hash, 0, 10, payee());
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { funder });
    BOOST_REQUIRE(block1 != system::null_hash);

    // The spender is unconfirmable by ancestry (its pooled parent conflicted).
    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    const auto spender = to_spend({ { parent.hash(false), 0 }, { funder.hash(false), 0 } }, 50);
    const auto replacement = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(archive_pool(query, spender));
    BOOST_REQUIRE(archive_confirm(query, block1, 2, { replacement }) != system::null_hash);

    const auto output = query.to_output(funder.hash(false), 0);
    BOOST_REQUIRE(query.is_spent(output));
    BOOST_REQUIRE(!query.is_confirmed_spent(output));
    BOOST_REQUIRE(!query.is_confirmable_spent(output));

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), funder.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).height, 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 1u);
}

BOOST_AUTO_TEST_CASE(query_address__get_unspent__mixed_spenders__expected)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto first = to_spend(system::one_hash, 0, 10, payee());
    const auto second = to_spend(system::one_hash, 1, 10, payee());
    const auto third = to_spend(system::one_hash, 2, 10, payee());
    const auto block1 = archive_confirm(query, test::genesis.hash(), 1, { first, second, third });
    BOOST_REQUIRE(block1 != system::null_hash);

    // Spent by a pooled, an unpooled, and a conflicted pooled tx respectively.
    const auto parent = to_spend(genesis_coinbase(), 0, 42);
    const auto replacement = to_spend(genesis_coinbase(), 0, 40);
    BOOST_REQUIRE(archive_pool(query, to_spend(first.hash(false), 0, 9)));
    BOOST_REQUIRE(query.set(to_spend(second.hash(false), 0, 9)));
    BOOST_REQUIRE(archive_pool(query, parent));
    BOOST_REQUIRE(archive_pool(query, to_spend({ { parent.hash(false), 0 }, { third.hash(false), 0 } }, 50)));
    BOOST_REQUIRE(archive_confirm(query, block1, 2, { replacement }) != system::null_hash);

    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE(!query.get_unspent(cancel, out, payee_key(), true));
    BOOST_REQUIRE_EQUAL(out.size(), 2u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), second.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).position, 2u);
    BOOST_REQUIRE_EQUAL(out.at(1).out.point().hash(), third.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(1).position, 3u);
}

// get_tx_unspent

BOOST_AUTO_TEST_CASE(query_address__get_tx_unspent__pooled__unconfirmed)
{
    settings settings{};
    settings.path = TEST_DIRECTORY;
    test::chunk_store store{ settings };
    test::query_accessor query{ store };
    BOOST_REQUIRE(!store.create(test::events_handler));
    BOOST_REQUIRE(query.initialize(test::genesis));
    store.set_pooling();

    const auto tx = to_spend(genesis_coinbase(), 0, 42, payee());
    BOOST_REQUIRE(archive_pool(query, tx));

    const auto entry = query.get_tx_unspent(query.to_output(tx.hash(false), 0));
    BOOST_REQUIRE(entry.valid());
    BOOST_REQUIRE_EQUAL(entry.height, unspent_output::unused_height);
    BOOST_REQUIRE_EQUAL(entry.position, unspent_output::unconfirmed_position);
    BOOST_REQUIRE_EQUAL(entry.out.point().hash(), tx.hash(false));
}

BOOST_AUTO_TEST_CASE(query_address__get_tx_unspent__unpooled__invalid_no_fault)
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

    const auto entry = query.get_tx_unspent(query.to_output(tx.hash(false), 0));
    BOOST_REQUIRE(!entry.valid());
    BOOST_REQUIRE(!entry.fault());
}

// duplicate instances (one tx archived twice, only one instance associated)

BOOST_AUTO_TEST_CASE(query_address__get_unspent__duplicate_instances__deduplicated)
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

    // block1a output 0 is spent by tx4, whose 0x08 output to the address is unspent.
    unspent_outputs out{};
    const std::atomic_bool cancel{};
    BOOST_REQUIRE_EQUAL(query.get_unspent(cancel, out, test::block1a_address0, true), error::success);
    BOOST_REQUIRE_EQUAL(out.size(), 1u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().hash(), test::tx4.hash(false));
    BOOST_REQUIRE_EQUAL(out.at(0).out.point().index(), 0u);
    BOOST_REQUIRE_EQUAL(out.at(0).out.value(), 0x08u);
    BOOST_REQUIRE_EQUAL(out.at(0).height, 2u);
    BOOST_REQUIRE_EQUAL(out.at(0).position, 0u);

    unspent_outputs confirmed{};
    BOOST_REQUIRE_EQUAL(query.get_confirmed_unspent(cancel, confirmed, test::block1a_address0, true), error::success);
    BOOST_REQUIRE_EQUAL(confirmed.size(), 1u);
    BOOST_REQUIRE_EQUAL(confirmed.at(0).out.point().hash(), test::tx4.hash(false));

    unspent_outputs unconfirmed{};
    BOOST_REQUIRE_EQUAL(query.get_unconfirmed_unspent(cancel, unconfirmed, test::block1a_address0, true), error::success);
    BOOST_REQUIRE(unconfirmed.empty());
}

BOOST_AUTO_TEST_SUITE_END()
