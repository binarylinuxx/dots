.pragma library

function normalize(value) {
    return String(value || "").toLowerCase().replace(/[^a-z0-9]+/g, " ").trim()
}

function haystackFor(item) {
    return normalize([
        item.name || "",
        item.label || "",
        item.keywords || ""
    ].join(" "))
}

function scoreText(haystack, query) {
    if (!query)
        return 1

    if (haystack.indexOf(query) !== -1)
        return 1000 - haystack.indexOf(query)

    let score = 0
    let searchFrom = 0
    let streak = 0

    for (let i = 0; i < query.length; i++) {
        const pos = haystack.indexOf(query[i], searchFrom)
        if (pos === -1)
            return 0

        streak = pos === searchFrom ? streak + 1 : 1
        score += 10 + streak * 5 - Math.min(pos - searchFrom, 8)
        searchFrom = pos + 1
    }

    return score
}

function filter(items, query) {
    const normalizedQuery = normalize(query)
    if (!normalizedQuery)
        return items

    return items.map(function(item) {
        const score = scoreText(haystackFor(item), normalizedQuery)
        const copy = {}
        for (const key in item)
            copy[key] = item[key]
        copy.score = score
        return copy
    }).filter(function(item) {
        return item.score > 0
    }).sort(function(a, b) {
        return b.score - a.score
    })
}
