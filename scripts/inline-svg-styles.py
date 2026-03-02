#!/usr/bin/env python3
"""Inline CSS <style> block classes into SVG element attributes for JUCE compatibility.

JUCE's SVG parser ignores <style> blocks — it only reads inline style="" attributes
and presentation attributes. This script extracts CSS class rules and applies them
directly to each element that references the class.

Usage: python3 inline-svg-styles.py input.svg output.svg
"""

import re
import sys
import xml.etree.ElementTree as ET


def parse_css_rules(style_text):
    """Parse CSS rules from a <style> block into {selector: {prop: value}} dict."""
    rules = {}
    # Match rules like .cls-1 { fill: url(#foo); }  or  .cls-1, .cls-2 { fill: red; }
    for match in re.finditer(r'([^{]+)\{([^}]+)\}', style_text):
        selectors = match.group(1).strip()
        declarations = match.group(2).strip()

        props = {}
        for decl in declarations.split(';'):
            decl = decl.strip()
            if ':' in decl:
                prop, val = decl.split(':', 1)
                props[prop.strip()] = val.strip()

        for sel in selectors.split(','):
            sel = sel.strip()
            if sel.startswith('.'):
                class_name = sel[1:]
                if class_name not in rules:
                    rules[class_name] = {}
                rules[class_name].update(props)
    return rules


# SVG presentation attributes that can be set directly (not just via style="")
SVG_PRESENTATION_ATTRS = {
    'fill', 'fill-rule', 'fill-opacity', 'stroke', 'stroke-width',
    'stroke-opacity', 'stroke-linecap', 'stroke-linejoin', 'stroke-dasharray',
    'stroke-dashoffset', 'opacity', 'clip-path', 'clip-rule', 'mask',
    'filter', 'display', 'visibility', 'font-family', 'font-size',
    'font-weight', 'font-style', 'text-anchor', 'text-decoration',
    'dominant-baseline', 'color', 'stop-color', 'stop-opacity',
    'flood-color', 'flood-opacity', 'lighting-color',
}

# Properties that go into style="" because they're CSS-only
CSS_ONLY_PROPS = {
    'isolation', 'mix-blend-mode',
}


def inline_styles(input_path, output_path):
    ET.register_namespace('', 'http://www.w3.org/2000/svg')
    ET.register_namespace('xlink', 'http://www.w3.org/1999/xlink')

    tree = ET.parse(input_path)
    root = tree.getroot()
    ns = 'http://www.w3.org/2000/svg'

    # Find and parse all <style> blocks
    all_rules = {}
    style_elements = root.findall(f'.//{{{ns}}}style')
    for style_el in style_elements:
        if style_el.text:
            all_rules.update(parse_css_rules(style_el.text))

    if not all_rules:
        print(f"No CSS rules found, copying as-is")
        tree.write(output_path, xml_declaration=True, encoding='unicode')
        return

    print(f"Found {len(all_rules)} CSS class rules")

    # Apply rules to every element with a class attribute
    for elem in root.iter():
        class_attr = elem.get('class')
        if not class_attr:
            continue

        classes = class_attr.split()
        existing_style = elem.get('style', '')
        style_parts = []
        if existing_style:
            style_parts.append(existing_style.rstrip(';'))

        for cls in classes:
            if cls not in all_rules:
                continue
            for prop, val in all_rules[cls].items():
                if prop in SVG_PRESENTATION_ATTRS:
                    # Set as direct attribute (only if not already set)
                    if elem.get(prop) is None:
                        elem.set(prop, val)
                else:
                    # Goes into style=""
                    style_parts.append(f'{prop}: {val}')

        if style_parts:
            elem.set('style', '; '.join(style_parts))

        # Remove class attribute (no longer needed)
        del elem.attrib['class']

    # Remove <style> blocks
    for style_el in style_elements:
        parent = find_parent(root, style_el)
        if parent is not None:
            parent.remove(style_el)

    tree.write(output_path, xml_declaration=True, encoding='unicode')
    print(f"Written to {output_path}")


def find_parent(root, target):
    """Find parent element of target in the tree."""
    for parent in root.iter():
        for child in parent:
            if child is target:
                return parent
    return None


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.svg output.svg")
        sys.exit(1)
    inline_styles(sys.argv[1], sys.argv[2])
