#ifndef OSMIUM_RELATIONS_RELATIONS_DATABASE_HPP
#define OSMIUM_RELATIONS_RELATIONS_DATABASE_HPP

/*

This file is part of Osmium (https://osmcode.org/libosmium).

Copyright 2013-2020 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <osmium/osm/relation.hpp>
#include <osmium/storage/item_stash.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

namespace osmium {

    namespace relations {

        class RelationHandle;

        /**
         * The RelationsDatabase is used for bringing relations and their
         * members together. It stores the relations in memory and keeps
         * track of how many members are needed to "complete" the relation.
         * It is intended to work together with the MembersDatabase template
         * class and usually used by relations manager classes.
         *
         * To access relations stored in the database a RelationHandle is
         * used. It is returned from the add() function. The handle is used
         * for all operations on the database contents, such as accessing
         * the stored relation, incrementing the member count or removing a
         * relation from the database.
         *
         * From the handle a "position" can be accessed, which, together with
         * the database object, can be turned into a handle again. The position
         * alone is smaller than the handle, so it can be stored elsewhere more
         * efficiently. Specifically this is used in the MembersDatabase.
         *
         * @code
         *    osmium::ItemStash stash;
         *    osmium::relations::RelationsDatabase db{stash};
         *    auto handle = db.add(relation);
         *    auto pos = handle.pos();
         *    auto second_handle = db[pos];
         * @endcode
         *
         * Now the `handle` and `second_handle` refer to the same relation.
         *
         * See the RelationHandle for information about what you can do with
         * it.
         */
        class RelationsDatabase {

            friend class RelationHandle;

            struct element {

                /// A handle to the relation in the ItemStash.
                osmium::ItemStash::handle_type handle;

                /**
                 * The number of members still needed before the relation is
                 * complete. This will be set to the number of members we are
                 * interested in (which can be all members of a relation or
                 * a subset of them) and then count down for every member we
                 * find. When it is 0, the relation is complete.
                 */
                std::size_t members;

            }; // struct element

            osmium::ItemStash& m_stash;
            std::vector<element> m_elements;

            osmium::Relation& get_relation(std::size_t pos) {
                assert(pos < m_elements.size());
                return m_stash.get<osmium::Relation>(m_elements[pos].handle);
            }

            /**
             * Access the number of members of the entry at the specified
             * position. This returns a reference so it can be changed.
             */
            std::size_t& members(std::size_t pos) noexcept {
                return m_elements[pos].members;
            }

            void remove(std::size_t pos) {
                auto& elem = m_elements[pos];
                m_stash.remove_item(elem.handle);
                elem = element{osmium::ItemStash::handle_type{}, 0};
            }

        public:

            /**
             * Construct a RelationsDatabase.
             *
             * @param stash Reference to an ItemStash object. All relations
             *              will be stored in this stash. It must be available
             *              until the RelationsDatabase is destroyed.
             */
            explicit RelationsDatabase(osmium::ItemStash& stash) :
                m_stash(stash) {
            }

            /**
             * Return an estimate of the number of bytes currently needed for
             * the RelationsDatabase. This does NOT include the memory used
             * in the stash. Used for debugging.
             *
             * Complexity: Constant.
             */
            std::size_t used_memory() const noexcept {
                return sizeof(element) * m_elements.capacity() +
                       sizeof(RelationsDatabase);
            }

            /**
             * The number of relations stored in the database. Includes
             * relations marked as removed.
             *
             * Complexity: Constant.
             */
            std::size_t size() const noexcept {
                return m_elements.size();
            }

            /**
             * Insert a relation into the database. The relation is copied
             * into the stash.
             *
             * Complexity: Amortized constant.
             *
             * @param relation The relation to be copied into the database.
             * @returns A handle to the relation.
             */
            RelationHandle add(const osmium::Relation& relation);

            /**
             * Return a handle to the relation at the specified position in
             * the database.
             *
             * Complexity: Constant.
             */
            RelationHandle operator[](std::size_t pos) noexcept;

            /**
             * Return the number of non-removed relations in the database.
             *
             * Complexity: Linear in the number of relations (as returned
             *             by size()).
             */
            std::size_t count_relations() const noexcept {
                return std::count_if(m_elements.cbegin(), m_elements.cend(), [&](const element& elem) {
                    return elem.handle.valid();
                });
            }

            /**
             * Iterate over all (not-removed) relations in the database.
             *
             * @tparam TFunc Function with type void(const RelationHandle&).
             * @param func Callback function which will be called for every
             *             not-removed relation with a RelationHandle.
             */
            template <typename TFunc>
            void for_each_relation(TFunc&& func);

        }; // RelationsDatabase

        /**
         * A RelationHandle is used to access elements in a RelationsDatabase.
         *
         * RelationHandles can not be created by user code, they are only
         * given out by a RelationsDatabase object.
         */
        class RelationHandle {

            friend class RelationsDatabase;

            RelationsDatabase* m_relation_database;
            std::size_t m_pos;

            RelationHandle(RelationsDatabase* relation_database, std::size_t pos) :
                m_relation_database(relation_database),
                m_pos(pos) {
            }

        public:

            /**
             * The RelationsDatabase this handle refers to.
             */
            RelationsDatabase* relation_database() const noexcept {
                return m_relation_database;
            }

            /**
             * The position of the element in the RelationsDatabase. Use the
             * RelationsDatabase::operator[] to get the handle back from this
             * position:
             * @code
             * auto pos = handle.pos();
             * auto second_handle = relation_db[pos];
             * @endcode
             */
            std::size_t pos() const noexcept {
                return m_pos;
            }

            /**
             * Access the relation stored in the database.
             */
            Relation& operator*() {
                return m_relation_database->get_relation(m_pos);
            }

            /**
             * Access the relation stored in the database.
             */
            const Relation& operator*() const {
                return m_relation_database->get_relation(m_pos);
            }

            /**
             * Call a function on the relation stored in the database.
             */
            Relation* operator->() {
                return &m_relation_database->get_relation(m_pos);
            }

            /**
             * Call a function on the relation stored in the database.
             */
            const Relation* operator->() const {
                return &m_relation_database->get_relation(m_pos);
            }

            /**
             * Remove the relation referred to by this handle from the database.
             * All handles referring to this database element become invalid.
             */
            void remove() {
                m_relation_database->remove(pos());
            }

            /**
             * Set the number of relation members that we want to track.
             */
            void set_members(std::size_t value) noexcept {
                m_relation_database->members(m_pos) = value;
            }

            /**
             * Increment the number of relation members that we want to track.
             */
            void increment_members() noexcept {
                ++(m_relation_database->members(m_pos));
            }

            /**
             * Decrement the number of relation members that we want to track.
             *
             * @pre @code has_all_members() == false @endcode
             */
            void decrement_members() noexcept {
                assert(m_relation_database->members(m_pos) > 0);
                --(m_relation_database->members(m_pos));
            }

            /**
             * Do we have all members? This is true if the number of tracked
             * members is zero.
             */
            bool has_all_members() const noexcept {
                return m_relation_database->members(m_pos) == 0;
            }

        }; // class RelationHandle

        inline RelationHandle RelationsDatabase::operator[](std::size_t pos) noexcept {
            assert(pos < m_elements.size());
            return {this, pos};
        }

        inline RelationHandle RelationsDatabase::add(const osmium::Relation& relation) {
            m_elements.push_back(element{m_stash.add_item(relation), 0});
            return {this, m_elements.size() - 1};
        }

        template <typename TFunc>
        void RelationsDatabase::for_each_relation(TFunc&& func) {
            for (std::size_t pos = 0; pos < m_elements.size(); ++pos) {
                if (m_elements[pos].handle.valid()) {
                    std::forward<TFunc>(func)(RelationHandle{this, pos});
                }
            }
        }

    } // namespace relations

} // namespace osmium

#endif // OSMIUM_RELATIONS_RELATIONS_DATABASE_HPP
