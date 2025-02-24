drop table if exists Items;
drop table if exists Bids;
drop table if exists Users;
drop table if exists Categories;

create table Items (
    ItemID          integer primary key,
    [Name]          text not null,
    Currently       real not null,
    Buy_Price       real,
    First_Bid       real not null,
    Number_of_Bids  integer not null,
    SellerID        text not null,
    [Started]       text not null,
    Ends            text not null,
    [Description]   text not null,
    foreign key (SellerID) references Users(UserID) on delete cascade
);

UPDATE Items
SET Buy_Price = NULL
WHERE Buy_Price = 'NULL';

create table Bids (
    ItemID      integer not null,
    UserID      text not null,
    [Time]      text not null,
    Amount      text not null,
    primary key (ItemID, UserID, Amount),
    foreign key (ItemID) references Items(ItemID) on delete cascade,
    foreign key (UserID) references Users(UserID) on delete cascade
);

create table Users (
    UserID      text primary key,
    Rating      integer not null,
    [Location]  text,
    Country     text
);

UPDATE Users
SET [Location] = NULL
WHERE [Location] ='NULL'

UPDATE Users
SET Country = NULL
WHERE Country = NULL

create table Categories (
    ItemID  integer not null,
    Category    text not null,
    primary key(ItemID, Category),
    foreign key (ItemID) references Items(ItemID) on delete cascade
);