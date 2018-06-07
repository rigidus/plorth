/*
 * Copyright (c) 2017-2018, Rauli Laine
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef PLORTH_GUI_STACK_DISPLAY_HPP_GUARD
#define PLORTH_GUI_STACK_DISPLAY_HPP_GUARD

#include <plorth/context.hpp>

#include <gtkmm.h>

namespace plorth
{
  namespace gui
  {
    class StackDisplayColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
      explicit StackDisplayColumns();

      inline const Gtk::TreeModelColumn<int>& index_column() const
      {
        return m_index_column;
      }

      inline const Gtk::TreeModelColumn<Glib::ustring>& value_column() const
      {
        return m_value_column;
      }

    private:
      Gtk::TreeModelColumn<int> m_index_column;
      Gtk::TreeModelColumn<Glib::ustring> m_value_column;
    };

    /**
     * Custom GTK widget which is used to display contents of Plorth execution
     * stack.
     */
    class StackDisplay : public Gtk::Bin
    {
    public:
      explicit StackDisplay();

      void update(const context::container_type& stack);

    private:
      Gtk::ScrolledWindow m_scrolled_window;
      Gtk::TreeView m_tree_view;
      StackDisplayColumns m_columns;
      Glib::RefPtr<Gtk::ListStore> m_tree_model;
    };
  }
}

#endif /* !PLORTH_GUI_STACK_DISPLAY_HPP_GUARD */
