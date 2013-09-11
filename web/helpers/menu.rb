module Menu
  def menu_link name, link
    if link == current_page.request_path
      content_tag('div', name, class: 'col-md-2 menu-item active')
    else
      content_tag('div', link_to(name, link), class: 'col-md-2 menu-item')
    end
  end
end