SELECT COUNT(DISTINCT Categories.Category)
FROM Categories
JOIN Items ON Categories.ItemID = Items.ItemID
WHERE Items.Currently > 100 AND Items.Number_of_Bids > 0;