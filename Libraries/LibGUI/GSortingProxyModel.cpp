/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/QuickSort.h>
#include <LibGUI/GAbstractView.h>
#include <LibGUI/GSortingProxyModel.h>
#include <stdio.h>
#include <stdlib.h>

GSortingProxyModel::GSortingProxyModel(NonnullRefPtr<GModel>&& target)
    : m_target(move(target))
    , m_key_column(-1)
{
    m_target->on_update = [this] {
        resort();
    };
}

GSortingProxyModel::~GSortingProxyModel()
{
}

int GSortingProxyModel::row_count(const GModelIndex& index) const
{
    return target().row_count(index);
}

int GSortingProxyModel::column_count(const GModelIndex& index) const
{
    return target().column_count(index);
}

GModelIndex GSortingProxyModel::map_to_target(const GModelIndex& index) const
{
    if (!index.is_valid())
        return {};
    if (index.row() >= m_row_mappings.size() || index.column() >= column_count())
        return {};
    return target().index(m_row_mappings[index.row()], index.column());
}

String GSortingProxyModel::row_name(int index) const
{
    return target().row_name(index);
}

String GSortingProxyModel::column_name(int index) const
{
    return target().column_name(index);
}

GModel::ColumnMetadata GSortingProxyModel::column_metadata(int index) const
{
    return target().column_metadata(index);
}

GVariant GSortingProxyModel::data(const GModelIndex& index, Role role) const
{
    return target().data(map_to_target(index), role);
}

void GSortingProxyModel::update()
{
    target().update();
}

StringView GSortingProxyModel::drag_data_type() const
{
    return target().drag_data_type();
}

void GSortingProxyModel::set_key_column_and_sort_order(int column, GSortOrder sort_order)
{
    if (column == m_key_column && sort_order == m_sort_order)
        return;

    ASSERT(column >= 0 && column < column_count());
    m_key_column = column;
    m_sort_order = sort_order;
    resort();
}

void GSortingProxyModel::resort()
{
    auto old_row_mappings = m_row_mappings;
    int row_count = target().row_count();
    m_row_mappings.resize(row_count);
    for (int i = 0; i < row_count; ++i)
        m_row_mappings[i] = i;
    if (m_key_column == -1) {
        did_update();
        return;
    }
    quick_sort(m_row_mappings.begin(), m_row_mappings.end(), [&](auto row1, auto row2) -> bool {
        auto data1 = target().data(target().index(row1, m_key_column), GModel::Role::Sort);
        auto data2 = target().data(target().index(row2, m_key_column), GModel::Role::Sort);
        if (data1 == data2)
            return 0;
        bool is_less_than;
        if (data1.is_string() && data2.is_string() && !m_sorting_case_sensitive)
            is_less_than = data1.as_string().to_lowercase() < data2.as_string().to_lowercase();
        else
            is_less_than = data1 < data2;
        return m_sort_order == GSortOrder::Ascending ? is_less_than : !is_less_than;
    });
    did_update();
    for_each_view([&](GAbstractView& view) {
        auto& selection = view.selection();
        Vector<GModelIndex> selected_indexes_in_target;
        selection.for_each_index([&](const GModelIndex& index) {
            selected_indexes_in_target.append(target().index(old_row_mappings[index.row()], index.column()));
        });

        selection.clear();
        for (auto& index : selected_indexes_in_target) {
            for (int i = 0; i < m_row_mappings.size(); ++i) {
                if (m_row_mappings[i] == index.row()) {
                    selection.add(this->index(i, index.column()));
                    continue;
                }
            }
        }
    });
}
